// Copyright 2021 The WebNN-native Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "examples/ResNet/ResNet.h"

#include <algorithm>

#include "common/Log.h"

ResNet::ResNet() : ExampleBase() {
}

bool ResNet::ParseAndCheckExampleOptions(int argc, const char* argv[]) {
    if (!ExampleBase::ParseAndCheckExampleOptions(argc, argv)) {
        return false;
    }
    bool nchw = mLayout == "nchw" ? true : false;
    mLabelPath = nchw ? "examples/labels/labels1000.txt" : "examples/labels/labels1001.txt";
    mModelHeight = nchw ? 224 : 299;
    mModelWidth = nchw ? 224 : 299;
    mModelChannels = 3;
    mNormalization = nchw ? true : false;
    nchw ? mMean = {0.485, 0.456, 0.406} : mMean = {127.5, 127.5, 127.5};
    nchw ? mStd = {0.229, 0.224, 0.225} : mStd = {127.5, 127.5, 127.5};
    mOutputShape = nchw ? std::vector<int32_t>({1, 1000}) : std::vector<int32_t>({1, 1001});
    return true;
}

const wnn::Operand ResNet::BuildConstantFromNpy(const wnn::GraphBuilder& builder,
                                                const std::string& path) {
    const cnpy::NpyArray data = cnpy::npy_load(path);
    mConstants.push_back(data.data_holder);
    return utils::BuildConstant(builder, data.shape, data.data<float>(), data.num_bytes());
}

const wnn::Operand ResNet::BuildNchwConv(const wnn::GraphBuilder& builder,
                                         const wnn::Operand& input,
                                         const std::string& name,
                                         const std::string& stageName,
                                         utils::Conv2dOptions* options) {
    std::string prefix = mWeightsPath;
    if (stageName != "") {
        prefix += "stage" + stageName + "_conv" + name;
    } else {
        prefix += "conv" + name;
    }
    const std::string weightsPath = prefix + "_weight.npy";
    const wnn::Operand convWeights = BuildConstantFromNpy(builder, weightsPath);
    const wnn::Conv2dOptions* conv2dOptions = options != nullptr ? options->AsPtr() : nullptr;
    return builder.Conv2d(input, convWeights, conv2dOptions);
}

const wnn::Operand ResNet::BuildNhwcConv(const wnn::GraphBuilder& builder,
                                         const wnn::Operand& input,
                                         const std::vector<std::string> nameIndices,
                                         utils::Conv2dOptions* options,
                                         bool relu) {
    std::string prefix = mWeightsPath;
    if (nameIndices[0] != "" && nameIndices[1] != "") {
        prefix += "block" + nameIndices[0] + "_unit_" + nameIndices[1] + "_bottleneck_v2_";
    }
    if (nameIndices[2] == "shortcut") {
        prefix += "shortcut";
    } else if (nameIndices[2] == "logits") {
        prefix += nameIndices[2];
    } else {
        prefix += "conv" + nameIndices[2];
    }
    const std::string weightsPath = prefix + "_weights.npy";
    const wnn::Operand convWeights = BuildConstantFromNpy(builder, weightsPath);
    const std::string biasPath = prefix + "_Conv2D_bias.npy";
    const wnn::Operand convBias = BuildConstantFromNpy(builder, biasPath);
    if (!mFused) {
        const wnn::Conv2dOptions* conv2dOptions = options != nullptr ? options->AsPtr() : nullptr;
        std::vector<int32_t> newShape = std::vector<int32_t>({1, 1, 1, -1});
        const wnn::Operand reshapedBias =
            builder.Reshape(convBias, newShape.data(), newShape.size());
        const wnn::Operand conv = builder.Conv2d(input, convWeights, conv2dOptions);
        const wnn::Operand add = builder.Add(conv, reshapedBias);
        if (relu) {
            return builder.Relu(add);
        }
        return add;
    } else {
        utils::Conv2dOptions fusedOptions;
        if (options != nullptr) {
            fusedOptions = *options;
        }
        fusedOptions.bias = convBias;
        if (relu) {
            fusedOptions.activation = builder.ReluOperator();
        }
        return builder.Conv2d(input, convWeights, fusedOptions.AsPtr());
    }
}

const wnn::Operand ResNet::BuildBatchNorm(const wnn::GraphBuilder& builder,
                                          const wnn::Operand& input,
                                          const std::string& name,
                                          const std::string& stageName,
                                          bool relu) {
    std::string prefix = mWeightsPath;
    if (stageName != "") {
        prefix += "stage" + stageName + "_batchnorm" + name;
    } else {
        prefix += "batchnorm" + name;
    }
    const wnn::Operand scale = BuildConstantFromNpy(builder, prefix + "_gamma.npy");
    const wnn::Operand bias = BuildConstantFromNpy(builder, prefix + "_beta.npy");
    const wnn::Operand mean = BuildConstantFromNpy(builder, prefix + "_running_mean.npy");
    const wnn::Operand variance = BuildConstantFromNpy(builder, prefix + "_running_var.npy");
    wnn::BatchNormOptions batchNormOptions;
    batchNormOptions.scale = scale;
    batchNormOptions.bias = bias;
    if (!mFused) {
        const wnn::Operand batchNorm = builder.BatchNorm(input, mean, variance, &batchNormOptions);
        if (relu) {
            return builder.Relu(batchNorm);
        }
        return batchNorm;
    } else {
        if (relu) {
            batchNormOptions.activation = builder.ReluOperator();
        }
        return builder.BatchNorm(input, mean, variance, &batchNormOptions);
    }
}

const wnn::Operand ResNet::BuildFusedBatchNorm(const wnn::GraphBuilder& builder,
                                               const wnn::Operand& input,
                                               const std::vector<std::string> nameIndices) {
    std::string prefix = mWeightsPath;
    if (nameIndices[0] == "postnorm") {
        prefix += "postnorm";
    } else {
        prefix += "block" + nameIndices[0] + "_unit_" + nameIndices[1] + "_bottleneck_v2_preact";
    }
    const std::string mulParamPath = prefix + "_FusedBatchNorm_mul_0_param.npy";
    const wnn::Operand mulParam = BuildConstantFromNpy(builder, mulParamPath);
    std::string addParamPath = prefix + "_FusedBatchNorm_add_param.npy";
    const wnn::Operand addParam = BuildConstantFromNpy(builder, addParamPath);
    return builder.Relu(builder.Add(builder.Mul(input, mulParam), addParam));
}

const wnn::Operand ResNet::BuildGemm(const wnn::GraphBuilder& builder,
                                     const wnn::Operand& input,
                                     const std::string& name) {
    std::string prefix = mWeightsPath + "dense" + name;
    std::string weightsPath = prefix + "_weight.npy";
    const wnn::Operand weights = BuildConstantFromNpy(builder, weightsPath);
    std::string biasPath = prefix + "_bias.npy";
    const wnn::Operand bias = BuildConstantFromNpy(builder, biasPath);
    wnn::GemmOptions gemmOptions;
    gemmOptions.c = bias;
    gemmOptions.bTranspose = true;
    return builder.Gemm(input, weights, &gemmOptions);
}

const wnn::Operand ResNet::BuildNchwBottlenectV2(const wnn::GraphBuilder& builder,
                                                 const wnn::Operand& input,
                                                 const std::string& stageName,
                                                 const std::vector<std::string> nameIndices,
                                                 bool downsample,
                                                 int32_t stride) {
    wnn::Operand residual = input;
    std::vector<int32_t> strides = {1, 1};
    if (downsample) {
        strides = {stride, stride};
    }
    const wnn::Operand bn1 = BuildBatchNorm(builder, input, nameIndices[0], stageName);
    const wnn::Operand conv1 = BuildNchwConv(builder, bn1, nameIndices[1], stageName);
    const wnn::Operand bn2 =
        BuildBatchNorm(builder, conv1, std::to_string(atoi(nameIndices[0].c_str()) + 1), stageName);
    utils::Conv2dOptions conv2Options;
    conv2Options.strides = strides;
    conv2Options.padding = {1, 1, 1, 1};
    const wnn::Operand conv2 =
        BuildNchwConv(builder, bn2, nameIndices[2], stageName, &conv2Options);
    const wnn::Operand bn3 =
        BuildBatchNorm(builder, conv2, std::to_string(atoi(nameIndices[0].c_str()) + 2), stageName);
    const wnn::Operand conv3 = BuildNchwConv(builder, bn3, nameIndices[3], stageName);
    if (downsample) {
        utils::Conv2dOptions convOptions;
        convOptions.strides = strides;
        residual = BuildNchwConv(builder, bn1, std::to_string(atoi(nameIndices[0].c_str()) + 3),
                                 stageName, &convOptions);
    }
    return builder.Add(conv3, residual);
}

std::vector<std::string> extendStringVector(std::vector<std::string> vector, std::string element) {
    vector.push_back(element);
    return vector;
}

const wnn::Operand ResNet::BuildNhwcBottlenectV2(const wnn::GraphBuilder& builder,
                                                 const wnn::Operand& input,
                                                 std::vector<std::string> nameIndices,
                                                 bool downsample,
                                                 bool shortcut) {
    wnn::Operand residual = input;
    const wnn::Operand fusedBn = BuildFusedBatchNorm(builder, input, nameIndices);
    utils::Conv2dOptions conv1Options;
    conv1Options.autoPad = wnn::AutoPad::SameUpper;
    conv1Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv1Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv1 =
        BuildNhwcConv(builder, fusedBn, extendStringVector(nameIndices, "1"), &conv1Options);
    if (downsample) {
        residual = BuildNhwcConv(builder, fusedBn, extendStringVector(nameIndices, "shortcut"),
                                 &conv1Options, false);
    }
    wnn::Operand conv2;
    utils::Conv2dOptions conv2Options;
    conv2Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv2Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    if (!downsample && shortcut) {
        utils::Pool2dOptions maxPoolOptions;
        maxPoolOptions.windowDimensions = {1, 1};
        maxPoolOptions.strides = {2, 2};
        maxPoolOptions.layout = wnn::InputOperandLayout::Nhwc;
        maxPoolOptions.autoPad = wnn::AutoPad::SameUpper;
        residual = builder.MaxPool2d(input, maxPoolOptions.AsPtr());
        const std::vector<uint32_t> constant = {0, 0, 1, 1, 1, 1, 0, 0};
        auto paddingData = std::make_shared<std::vector<char>>(constant.size() * sizeof(uint32_t));
        std::memcpy(paddingData->data(), constant.data(), constant.size() * sizeof(int32_t));
        mConstants.push_back(paddingData);
        const wnn::Operand padding = utils::BuildConstant(
            builder, {4, 2}, paddingData->data(), paddingData->size(), wnn::OperandType::Uint32);
        const wnn::Operand pad = builder.Pad(conv1, padding);
        conv2Options.strides = {2, 2};
        conv2 = BuildNhwcConv(builder, pad, extendStringVector(nameIndices, "2"), &conv2Options);
    } else {
        conv2Options.autoPad = wnn::AutoPad::SameUpper;
        conv2 = BuildNhwcConv(builder, conv1, extendStringVector(nameIndices, "2"), &conv2Options);
    }
    utils::Conv2dOptions conv3Options;
    conv3Options.autoPad = wnn::AutoPad::SameUpper;
    conv3Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv3Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv3 =
        BuildNhwcConv(builder, conv2, extendStringVector(nameIndices, "3"), &conv3Options, false);
    return builder.Add(conv3, residual);
}

const wnn::Operand ResNet::loop(const wnn::GraphBuilder& builder,
                                const wnn::Operand node,
                                uint32_t num) {
    if (num > 22) {
        return node;
    } else {
        const wnn::Operand newNode =
            BuildNhwcBottlenectV2(builder, node, {"3", std::to_string(num)}, false, false);
        num++;
        return loop(builder, newNode, num);
    }
}

const wnn::Operand ResNet::LoadNCHW(const wnn::GraphBuilder& builder, bool softmax) {
    mWeightsPath = mWeightsPath + "resnetv24_";
    const wnn::Operand input = utils::BuildInput(builder, "input", {1, 3, 224, 224});

    const wnn::Operand bn1 = BuildBatchNorm(builder, input, "0", "", false);
    utils::Conv2dOptions conv0Options;
    conv0Options.padding = {3, 3, 3, 3};
    conv0Options.strides = {2, 2};
    const wnn::Operand conv0 = BuildNchwConv(builder, bn1, "0", "", &conv0Options);
    const wnn::Operand bn2 = BuildBatchNorm(builder, conv0, "1", "");
    utils::Pool2dOptions maxPoolOptions;
    maxPoolOptions.windowDimensions = {3, 3};
    maxPoolOptions.padding = {1, 1, 1, 1};
    maxPoolOptions.strides = {2, 2};
    const wnn::Operand pool1 = builder.MaxPool2d(bn2, maxPoolOptions.AsPtr());

    // Stage 1
    const wnn::Operand bottleneck1 =
        BuildNchwBottlenectV2(builder, pool1, "1", {"0", "0", "1", "2"}, true);
    const wnn::Operand bottleneck2 =
        BuildNchwBottlenectV2(builder, bottleneck1, "1", {"3", "4", "5", "6"});
    const wnn::Operand bottleneck3 =
        BuildNchwBottlenectV2(builder, bottleneck2, "1", {"6", "7", "8", "9"});

    // Stage 2
    const wnn::Operand bottleneck4 =
        BuildNchwBottlenectV2(builder, bottleneck3, "2", {"0", "0", "1", "2"}, true, 2);
    const wnn::Operand bottleneck5 =
        BuildNchwBottlenectV2(builder, bottleneck4, "2", {"3", "4", "5", "6"});
    const wnn::Operand bottleneck6 =
        BuildNchwBottlenectV2(builder, bottleneck5, "2", {"6", "7", "8", "9"});
    const wnn::Operand bottleneck7 =
        BuildNchwBottlenectV2(builder, bottleneck6, "2", {"9", "10", "11", "12"});

    // Stage 3
    const wnn::Operand bottleneck8 =
        BuildNchwBottlenectV2(builder, bottleneck7, "3", {"0", "0", "1", "2"}, true, 2);
    const wnn::Operand bottleneck9 =
        BuildNchwBottlenectV2(builder, bottleneck8, "3", {"3", "4", "5", "6"});
    const wnn::Operand bottleneck10 =
        BuildNchwBottlenectV2(builder, bottleneck9, "3", {"6", "7", "8", "9"});
    const wnn::Operand bottleneck11 =
        BuildNchwBottlenectV2(builder, bottleneck10, "3", {"9", "10", "11", "12"});
    const wnn::Operand bottleneck12 =
        BuildNchwBottlenectV2(builder, bottleneck11, "3", {"12", "13", "14", "15"});
    const wnn::Operand bottleneck13 =
        BuildNchwBottlenectV2(builder, bottleneck12, "3", {"15", "16", "17", "18"});

    // Stage 4
    const wnn::Operand bottleneck14 =
        BuildNchwBottlenectV2(builder, bottleneck13, "4", {"0", "0", "1", "2"}, true, 2);
    const wnn::Operand bottleneck15 =
        BuildNchwBottlenectV2(builder, bottleneck14, "4", {"3", "4", "5", "6"});
    const wnn::Operand bottleneck16 =
        BuildNchwBottlenectV2(builder, bottleneck15, "4", {"6", "7", "8", "9"});

    const wnn::Operand bn3 = BuildBatchNorm(builder, bottleneck16, "2", "");
    const wnn::Operand pool2 = builder.AveragePool2d(bn3);
    const std::vector<int32_t> newShape = {1, -1};
    const wnn::Operand reshape = builder.Reshape(pool2, newShape.data(), newShape.size());
    const wnn::Operand gemm = BuildGemm(builder, reshape, "0");
    const wnn::Operand output = softmax ? builder.Softmax(gemm) : gemm;
    return output;
}

const wnn::Operand ResNet::LoadNHWC(const wnn::GraphBuilder& builder, bool softmax) {
    mWeightsPath = mWeightsPath + "resnet_v2_101_";
    const wnn::Operand input = utils::BuildInput(builder, "input", {1, 299, 299, 3});

    const std::vector<uint32_t> constant = {0, 0, 3, 3, 3, 3, 0, 0};
    auto paddingData = std::make_shared<std::vector<char>>(constant.size() * sizeof(uint32_t));
    std::memcpy(paddingData->data(), constant.data(), constant.size() * sizeof(int32_t));
    mConstants.push_back(paddingData);
    const wnn::Operand padding = utils::BuildConstant(
        builder, {4, 2}, paddingData->data(), paddingData->size(), wnn::OperandType::Uint32);
    const wnn::Operand pad = builder.Pad(input, padding);
    utils::Conv2dOptions conv1Options;
    conv1Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv1Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    conv1Options.strides = {2, 2};
    const wnn::Operand conv1 = BuildNhwcConv(builder, pad, {"", "", "1"}, &conv1Options, false);
    utils::Pool2dOptions maxPoolOptions;
    maxPoolOptions.windowDimensions = {3, 3};
    maxPoolOptions.strides = {2, 2};
    maxPoolOptions.autoPad = wnn::AutoPad::SameUpper;
    maxPoolOptions.layout = wnn::InputOperandLayout::Nhwc;
    const wnn::Operand pool = builder.MaxPool2d(conv1, maxPoolOptions.AsPtr());

    // Block 1
    const wnn::Operand bottleneck1 = BuildNhwcBottlenectV2(builder, pool, {"1", "1"}, true);
    const wnn::Operand bottleneck2 =
        BuildNhwcBottlenectV2(builder, bottleneck1, {"1", "2"}, false, false);
    const wnn::Operand bottleneck3 = BuildNhwcBottlenectV2(builder, bottleneck2, {"1", "3"});

    // Block 2
    const wnn::Operand bottleneck4 = BuildNhwcBottlenectV2(builder, bottleneck3, {"2", "1"}, true);
    const wnn::Operand bottleneck5 =
        BuildNhwcBottlenectV2(builder, bottleneck4, {"2", "2"}, false, false);
    const wnn::Operand bottleneck6 =
        BuildNhwcBottlenectV2(builder, bottleneck5, {"2", "3"}, false, false);
    const wnn::Operand bottleneck7 = BuildNhwcBottlenectV2(builder, bottleneck6, {"2", "4"});

    // Block 3
    const wnn::Operand bottleneck8 = BuildNhwcBottlenectV2(builder, bottleneck7, {"3", "1"}, true);
    const wnn::Operand bottleneck9 = loop(builder, bottleneck8, 2);
    const wnn::Operand bottleneck10 = BuildNhwcBottlenectV2(builder, bottleneck9, {"3", "23"});

    // Block 4
    const wnn::Operand bottleneck11 =
        BuildNhwcBottlenectV2(builder, bottleneck10, {"4", "1"}, true);
    const wnn::Operand bottleneck12 =
        BuildNhwcBottlenectV2(builder, bottleneck11, {"4", "2"}, false, false);
    const wnn::Operand bottleneck13 =
        BuildNhwcBottlenectV2(builder, bottleneck12, {"4", "3"}, false, false);
    const wnn::Operand fusedBn = BuildFusedBatchNorm(builder, bottleneck13, {"postnorm"});
    wnn::ReduceOptions reduceOptions;
    const std::vector<int32_t> axes = {1, 2};
    reduceOptions.axes = axes.data();
    reduceOptions.axesCount = axes.size();
    reduceOptions.keepDimensions = true;
    const wnn::Operand mean = builder.ReduceMean(fusedBn, &reduceOptions);
    utils::Conv2dOptions conv2Options;
    conv2Options.autoPad = wnn::AutoPad::SameUpper;
    conv2Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv2Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv2 =
        BuildNhwcConv(builder, mean, {"", "", "logits"}, &conv2Options, false);
    const std::vector<int32_t> newShape = {1, -1};
    const wnn::Operand reshape = builder.Reshape(conv2, newShape.data(), newShape.size());
    const wnn::Operand output = softmax ? builder.Softmax(reshape) : reshape;
    return output;
}
