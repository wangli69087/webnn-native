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

#include "examples/MobileNetV2/MobileNetV2.h"

#include <algorithm>

#include "common/Log.h"

MobileNetV2::MobileNetV2() : ExampleBase() {
}

bool MobileNetV2::ParseAndCheckExampleOptions(int argc, const char* argv[]) {
    if (!ExampleBase::ParseAndCheckExampleOptions(argc, argv)) {
        return false;
    }

    bool nchw = mLayout == "nchw" ? true : false;
    mLabelPath = nchw ? "examples/labels/labels1000.txt" : "examples/labels/labels1001.txt";
    mModelHeight = 224;
    mModelWidth = 224;
    mModelChannels = 3;
    mNormalization = nchw ? true : false;
    nchw ? mMean = {0.485, 0.456, 0.406} : mMean = {127.5, 127.5, 127.5};
    nchw ? mStd = {0.229, 0.224, 0.225} : mStd = {127.5, 127.5, 127.5};
    mOutputShape = nchw ? std::vector<int32_t>({1, 1000}) : std::vector<int32_t>({1, 1001});
    return true;
}

const wnn::Operand MobileNetV2::BuildConstantFromNpy(const wnn::GraphBuilder& builder,
                                                     const std::string& path) {
    const cnpy::NpyArray data = cnpy::npy_load(path);
    mConstants.push_back(data.data_holder);
    return utils::BuildConstant(builder, data.shape, data.data<float>(), data.num_bytes());
}

const wnn::Operand MobileNetV2::BuildConv(const wnn::GraphBuilder& builder,
                                          const wnn::Operand& input,
                                          int32_t convIndex,
                                          bool relu6,
                                          utils::Conv2dOptions* options,
                                          const std::string& biasName) {
    std::string prefix = mLayout == "nchw" ? mWeightsPath + "conv_" : mWeightsPath + "Const_";
    std::string suffix = mLayout == "nchw" ? "_weight.npy" : ".npy";
    const std::string weightsPath = prefix + std::to_string(convIndex) + suffix;
    const wnn::Operand convWeights = BuildConstantFromNpy(builder, weightsPath);

    prefix = mLayout == "nchw" ? mWeightsPath + "conv_" : mWeightsPath + "MobilenetV2_";
    if (mLayout == "nchw") {
        prefix.append(std::to_string(convIndex));
    }
    const std::string biasPath = prefix + biasName + "_bias.npy";
    const wnn::Operand convBias = BuildConstantFromNpy(builder, biasPath);
    wnn::ClampOptions clampOptions = {0, 6};
    if (!mFused) {
        std::vector<int32_t> newShape = mLayout == "nchw" ? std::vector<int32_t>({1, -1, 1, 1})
                                                          : std::vector<int32_t>({1, 1, 1, -1});
        const wnn::Operand reshapedBias =
            builder.Reshape(convBias, newShape.data(), newShape.size());

        const wnn::Conv2dOptions* conv2dOptions = options != nullptr ? options->AsPtr() : nullptr;
        const wnn::Operand conv = builder.Conv2d(input, convWeights, conv2dOptions);
        const wnn::Operand add = builder.Add(conv, reshapedBias);
        if (relu6) {
            return builder.Clamp(add, &clampOptions);
        } else {
            return add;
        }
    } else {
        utils::Conv2dOptions fusedOptions;
        if (options != nullptr) {
            fusedOptions = *options;
        }
        fusedOptions.bias = convBias;
        if (relu6) {
            fusedOptions.activation = builder.ClampOperator(&clampOptions);
        }
        return builder.Conv2d(input, convWeights, fusedOptions.AsPtr());
    }
}

const wnn::Operand MobileNetV2::BuildConvBatchNorm(const wnn::GraphBuilder& builder,
                                                   const wnn::Operand& input,
                                                   int32_t nameIndex,
                                                   bool relu,
                                                   utils::Conv2dOptions* options,
                                                   int32_t subNameIndex) {
    const std::string subName =
        subNameIndex != -1 ? "_linearbottleneck" + std::to_string(subNameIndex) : "";
    std::string prefix = mWeightsPath + "mobilenetv20_features" + subName;
    const std::string weightsPath = prefix + "_conv" + std::to_string(nameIndex) + "_weight.npy";
    const wnn::Operand convWeights = BuildConstantFromNpy(builder, weightsPath);
    prefix.append("_batchnorm" + std::to_string(nameIndex));
    const std::string meanPath = prefix + "_running_mean.npy";
    const wnn::Operand mean = BuildConstantFromNpy(builder, meanPath);
    const std::string variancePath = prefix + "_running_var.npy";
    const wnn::Operand variance = BuildConstantFromNpy(builder, variancePath);
    const wnn::Conv2dOptions* conv2dOptions = options != nullptr ? options->AsPtr() : nullptr;
    const wnn::Operand conv = builder.Conv2d(input, convWeights, conv2dOptions);
    const std::string scalePath = prefix + "_gamma.npy";
    const wnn::Operand scale = BuildConstantFromNpy(builder, scalePath);
    const std::string biasPath = prefix + "_beta.npy";
    const wnn::Operand bias = BuildConstantFromNpy(builder, biasPath);
    wnn::BatchNormOptions batchNormOptions;
    batchNormOptions.scale = scale;
    batchNormOptions.bias = bias;
    if (mFused) {
        if (relu) {
            batchNormOptions.activation = builder.ReluOperator();
        }
    }
    auto batchNorm = builder.BatchNorm(conv, mean, variance, &batchNormOptions);
    if (!mFused) {
        if (relu) {
            batchNorm = builder.Relu(batchNorm);
        }
    }
    return batchNorm;
}

const wnn::Operand MobileNetV2::BuildGemm(const wnn::GraphBuilder& builder,
                                          const wnn::Operand& input,
                                          int32_t gemmIndex) {
    std::string suffix = mLayout == "nchw" ? "_weight.npy" : "_kernel.npy";
    const std::string weightsPath = mWeightsPath + "gemm_" + std::to_string(gemmIndex) + suffix;
    const wnn::Operand gemmWeights = BuildConstantFromNpy(builder, weightsPath);
    const std::string biasPath = mWeightsPath + "gemm_" + std::to_string(gemmIndex) + "_bias.npy";
    const wnn::Operand gemmBias = BuildConstantFromNpy(builder, biasPath);
    wnn::GemmOptions gemmOptions;
    gemmOptions.c = gemmBias;
    gemmOptions.bTranspose = true;
    return builder.Gemm(input, gemmWeights, &gemmOptions);
}

const wnn::Operand MobileNetV2::BuildFire(const wnn::GraphBuilder& builder,
                                          const wnn::Operand& input,
                                          const std::vector<int32_t>& convIndexes,
                                          int32_t groups,
                                          bool strides,
                                          bool shouldAdd) {
    utils::Conv2dOptions convOptions;
    if (!(mLayout == "nchw")) {
        convOptions.inputLayout = wnn::InputOperandLayout::Nhwc;
        convOptions.filterLayout = wnn::Conv2dFilterOperandLayout::Hwio;
    }
    const wnn::Operand conv1x1 = BuildConv(builder, input, convIndexes[0], true, &convOptions);
    convOptions.padding = {1, 1, 1, 1};
    convOptions.groups = groups;
    if (strides) {
        convOptions.strides = {2, 2};
    }
    const wnn::Operand conv3x3 = BuildConv(builder, conv1x1, convIndexes[1], true, &convOptions);
    const wnn::Operand conv1x1NotClamp = BuildConv(builder, conv3x3, convIndexes[2], false);
    return shouldAdd ? builder.Add(input, conv1x1NotClamp) : conv1x1NotClamp;
}

const wnn::Operand MobileNetV2::BuildBatchNormFire(const wnn::GraphBuilder& builder,
                                                   const wnn::Operand& input,
                                                   int32_t subNameIndex,
                                                   utils::Conv2dOptions* options) {
    const wnn::Operand batchNorm0 =
        BuildConvBatchNorm(builder, input, 0, true, nullptr, subNameIndex);
    const wnn::Operand batchNorm1 =
        BuildConvBatchNorm(builder, batchNorm0, 1, true, options, subNameIndex);
    return BuildConvBatchNorm(builder, batchNorm1, 2, false, nullptr, subNameIndex);
}

const wnn::Operand MobileNetV2::BuildLinearBottleneck(const wnn::GraphBuilder& builder,
                                                      const wnn::Operand& input,
                                                      const std::vector<int32_t>& convIndexes,
                                                      int32_t biasIndex,
                                                      utils::Conv2dOptions* dwiseOptions,
                                                      bool shouldAdd) {
    utils::Conv2dOptions convOptions;
    convOptions.autoPad = wnn::AutoPad::SameUpper;
    convOptions.inputLayout = wnn::InputOperandLayout::Nhwc;
    convOptions.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;

    const std::string biasPrefix = "expanded_conv_" + std::to_string(biasIndex);

    const wnn::Operand conv1x1 = BuildConv(builder, input, convIndexes[0], true, &convOptions,
                                           biasPrefix + "_expand_Conv2D");
    dwiseOptions->autoPad = wnn::AutoPad::SameUpper;
    dwiseOptions->inputLayout = wnn::InputOperandLayout::Nhwc;
    dwiseOptions->filterLayout = wnn::Conv2dFilterOperandLayout::Ihwo;
    const wnn::Operand conv3x3 = BuildConv(builder, conv1x1, convIndexes[1], true, dwiseOptions,
                                           biasPrefix + "_depthwise_depthwise");

    const wnn::Operand conv1x1NotClamp = BuildConv(builder, conv3x3, convIndexes[2], false,
                                                   &convOptions, biasPrefix + "_project_Conv2D");
    return shouldAdd ? builder.Add(input, conv1x1NotClamp) : conv1x1NotClamp;
}

const wnn::Operand MobileNetV2::BuildFireMore(const wnn::GraphBuilder& builder,
                                              const wnn::Operand& input,
                                              const std::vector<int32_t>& convIndexes,
                                              const std::vector<int32_t> groups,
                                              bool strides) {
    const std::vector<int32_t> convList1(convIndexes.begin(), convIndexes.begin() + 3);
    const wnn::Operand fire1 = BuildFire(builder, input, convList1, groups[0], strides, false);
    const std::vector<int32_t> convList2(convIndexes.begin() + 3, convIndexes.begin() + 6);
    const wnn::Operand fire2 = BuildFire(builder, fire1, convList2, groups[1]);
    if (convIndexes.size() >= 9) {
        const std::vector<int32_t> convList3(convIndexes.begin() + 6, convIndexes.begin() + 9);
        const wnn::Operand fire3 = BuildFire(builder, fire2, convList3, groups[1]);
        if (convIndexes.size() == 12) {
            const std::vector<int32_t> convList4(convIndexes.begin() + 9, convIndexes.begin() + 12);
            return BuildFire(builder, fire3, convList4, groups[1]);
        } else {
            return fire3;
        }
    } else {
        return fire2;
    }
}

const wnn::Operand MobileNetV2::LoadNCHW(const wnn::GraphBuilder& builder, bool softmax) {
    mWeightsPath = mWeightsPath;

    const wnn::Operand input = utils::BuildInput(builder, "input", {1, 3, 224, 224});

    utils::Conv2dOptions conv0Options;
    conv0Options.strides = {2, 2};
    conv0Options.padding = {1, 1, 1, 1};
    const wnn::Operand conv0 = BuildConv(builder, input, 0, true, &conv0Options);

    utils::Conv2dOptions conv2Options;
    conv2Options.groups = 32;
    conv2Options.padding = {1, 1, 1, 1};
    const wnn::Operand conv2 = BuildConv(builder, conv0, 2, true, &conv2Options);
    const wnn::Operand conv4 = BuildConv(builder, conv2, 4, false);
    const wnn::Operand add15 = BuildFireMore(builder, conv4, {5, 7, 9, 10, 12, 14}, {96, 144});
    const wnn::Operand add32 =
        BuildFireMore(builder, add15, {16, 18, 20, 21, 23, 25, 27, 29, 31}, {144, 192});
    const wnn::Operand add55 =
        BuildFireMore(builder, add32, {33, 35, 37, 38, 40, 42, 44, 46, 48, 50, 52, 54}, {192, 384});
    const wnn::Operand add72 =
        BuildFireMore(builder, add55, {56, 58, 60, 61, 63, 65, 67, 69, 71}, {384, 576}, false);
    const wnn::Operand add89 =
        BuildFireMore(builder, add72, {73, 75, 77, 78, 80, 82, 84, 86, 88}, {576, 960});
    const wnn::Operand conv94 = BuildFire(builder, add89, {90, 92, 94}, 960, false, false);
    const wnn::Operand conv95 = BuildConv(builder, conv94, 95, true);
    const wnn::Operand pool97 = builder.AveragePool2d(conv95);
    const std::vector<int32_t> newShape = {1, -1};
    const wnn::Operand reshape103 = builder.Reshape(pool97, newShape.data(), newShape.size());
    const wnn::Operand gemm104 = BuildGemm(builder, reshape103, 104);
    const wnn::Operand output = softmax ? builder.Softmax(gemm104) : gemm104;
    return output;
}

const wnn::Operand MobileNetV2::LoadNHWC(const wnn::GraphBuilder& builder, bool softmax) {
    mWeightsPath = mWeightsPath;
    const wnn::Operand input = utils::BuildInput(builder, "input", {1, 224, 224, 3});

    utils::Conv2dOptions conv0Options;
    conv0Options.strides = {2, 2};
    conv0Options.autoPad = wnn::AutoPad::SameUpper;
    conv0Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv0Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv0 = BuildConv(builder, input, 90, true, &conv0Options, "Conv_Conv2D");

    utils::Conv2dOptions conv1Options;
    conv1Options.groups = 32;
    conv1Options.autoPad = wnn::AutoPad::SameUpper;
    conv1Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv1Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ihwo;
    const wnn::Operand conv1 =
        BuildConv(builder, conv0, 238, true, &conv1Options, "expanded_conv_depthwise_depthwise");

    utils::Conv2dOptions conv2Options;
    conv2Options.autoPad = wnn::AutoPad::SameUpper;
    conv2Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv2Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv2 =
        BuildConv(builder, conv1, 167, false, &conv2Options, "expanded_conv_project_Conv2D");

    utils::Conv2dOptions dwiseConv0Options;
    dwiseConv0Options.groups = 96;
    dwiseConv0Options.strides = {2, 2};
    const wnn::Operand bottleneck0 =
        BuildLinearBottleneck(builder, conv2, {165, 99, 73}, 1, &dwiseConv0Options, false);

    utils::Conv2dOptions dwiseConv1Options;
    dwiseConv1Options.groups = 144;
    const wnn::Operand bottleneck1 =
        BuildLinearBottleneck(builder, bottleneck0, {3, 119, 115}, 2, &dwiseConv1Options);

    utils::Conv2dOptions dwiseConv2Options;
    dwiseConv2Options.groups = 144;
    dwiseConv2Options.strides = {2, 2};
    const wnn::Operand bottleneck2 =
        BuildLinearBottleneck(builder, bottleneck1, {255, 216, 157}, 3, &dwiseConv2Options, false);

    utils::Conv2dOptions dwiseConv3Options;
    dwiseConv3Options.groups = 192;
    const wnn::Operand bottleneck3 =
        BuildLinearBottleneck(builder, bottleneck2, {227, 221, 193}, 4, &dwiseConv3Options);

    utils::Conv2dOptions dwiseConv4Options = dwiseConv3Options;
    const wnn::Operand bottleneck4 =
        BuildLinearBottleneck(builder, bottleneck3, {243, 102, 215}, 5, &dwiseConv4Options);

    utils::Conv2dOptions dwiseConv5Options;
    dwiseConv5Options.groups = 192;
    dwiseConv5Options.strides = {2, 2};
    const wnn::Operand bottleneck5 =
        BuildLinearBottleneck(builder, bottleneck4, {226, 163, 229}, 6, &dwiseConv5Options, false);

    utils::Conv2dOptions dwiseConv6Options;
    dwiseConv6Options.groups = 384;
    const wnn::Operand bottleneck6 =
        BuildLinearBottleneck(builder, bottleneck5, {104, 254, 143}, 7, &dwiseConv6Options);

    utils::Conv2dOptions dwiseConv7Options;
    dwiseConv7Options.groups = 384;
    const wnn::Operand bottleneck7 =
        BuildLinearBottleneck(builder, bottleneck6, {25, 142, 202}, 8, &dwiseConv7Options);

    utils::Conv2dOptions dwiseConv8Options = dwiseConv7Options;
    const wnn::Operand bottleneck8 =
        BuildLinearBottleneck(builder, bottleneck7, {225, 129, 98}, 9, &dwiseConv8Options);

    utils::Conv2dOptions dwiseConv9Options = dwiseConv7Options;
    const wnn::Operand bottleneck9 =
        BuildLinearBottleneck(builder, bottleneck8, {169, 2, 246}, 10, &dwiseConv9Options, false);

    utils::Conv2dOptions dwiseConv10Options;
    dwiseConv10Options.groups = 576;
    const wnn::Operand bottleneck10 =
        BuildLinearBottleneck(builder, bottleneck9, {162, 87, 106}, 11, &dwiseConv10Options);

    utils::Conv2dOptions dwiseConv11Options = dwiseConv10Options;
    const wnn::Operand bottleneck11 =
        BuildLinearBottleneck(builder, bottleneck10, {52, 22, 40}, 12, &dwiseConv11Options);

    utils::Conv2dOptions dwiseConv12Options;
    dwiseConv12Options.groups = 576;
    dwiseConv12Options.strides = {2, 2};
    const wnn::Operand bottleneck12 = BuildLinearBottleneck(builder, bottleneck11, {114, 65, 242},
                                                            13, &dwiseConv12Options, false);

    utils::Conv2dOptions dwiseConv13Options;
    dwiseConv13Options.groups = 960;
    const wnn::Operand bottleneck13 =
        BuildLinearBottleneck(builder, bottleneck12, {203, 250, 92}, 14, &dwiseConv13Options);

    utils::Conv2dOptions dwiseConv14Options = dwiseConv13Options;
    const wnn::Operand bottleneck14 =
        BuildLinearBottleneck(builder, bottleneck13, {133, 130, 258}, 15, &dwiseConv14Options);

    utils::Conv2dOptions dwiseConv15Options = dwiseConv13Options;
    const wnn::Operand bottleneck15 = BuildLinearBottleneck(builder, bottleneck14, {60, 248, 100},
                                                            16, &dwiseConv15Options, false);

    utils::Conv2dOptions conv3Options;
    conv3Options.autoPad = wnn::AutoPad::SameUpper;
    conv3Options.inputLayout = wnn::InputOperandLayout::Nhwc;
    conv3Options.filterLayout = wnn::Conv2dFilterOperandLayout::Ohwi;
    const wnn::Operand conv3 =
        BuildConv(builder, bottleneck15, 71, true, &conv3Options, "Conv_1_Conv2D");

    utils::Pool2dOptions poolOptions;
    poolOptions.windowDimensions = {7, 7};
    poolOptions.layout = wnn::InputOperandLayout::Nhwc;
    const wnn::Operand averagePool2d = builder.AveragePool2d(conv3, poolOptions.AsPtr());

    utils::Conv2dOptions conv4Options = conv3Options;
    const wnn::Operand conv4 =
        BuildConv(builder, averagePool2d, 222, false, &conv3Options, "Logits_Conv2d_1c_1x1_Conv2D");

    const std::vector<int32_t> newShape = {1, -1};
    const wnn::Operand reshape = builder.Reshape(conv4, newShape.data(), newShape.size());
    const wnn::Operand output = softmax ? builder.Softmax(reshape) : reshape;
    return output;
}

const wnn::Operand MobileNetV2::LoadBatchNormNCHW(const wnn::GraphBuilder& builder, bool softmax) {
    mWeightsPath = mWeightsPath;
    const std::vector<int32_t> padding = {1, 1, 1, 1};
    const std::vector<int32_t> strides = {2, 2};
    const wnn::Operand input = utils::BuildInput(builder, "input", {1, 3, 224, 224});
    utils::Conv2dOptions conv0Options;
    conv0Options.padding = padding;
    conv0Options.strides = strides;
    const wnn::Operand batchNorm0 = BuildConvBatchNorm(builder, input, 0, true, &conv0Options);
    utils::Conv2dOptions fire0Options;
    fire0Options.padding = padding;
    fire0Options.groups = 32;
    const wnn::Operand fire0 = BuildBatchNormFire(builder, batchNorm0, 0, &fire0Options);
    utils::Conv2dOptions fire1Options;
    fire1Options = conv0Options;
    fire1Options.groups = 96;
    const wnn::Operand fire1 = BuildBatchNormFire(builder, fire0, 1, &fire1Options);
    utils::Conv2dOptions fire2Options;
    fire2Options.padding = padding;
    fire2Options.groups = 144;
    const wnn::Operand fire2 = BuildBatchNormFire(builder, fire1, 2, &fire2Options);
    const wnn::Operand add0 = builder.Add(fire1, fire2);
    utils::Conv2dOptions fire3Options = conv0Options;
    fire3Options.groups = 144;
    const wnn::Operand fire3 = BuildBatchNormFire(builder, add0, 3, &fire3Options);
    utils::Conv2dOptions fire4Options;
    fire4Options.padding = padding;
    fire4Options.groups = 192;
    const wnn::Operand fire4 = BuildBatchNormFire(builder, fire3, 4, &fire4Options);
    const wnn::Operand add1 = builder.Add(fire3, fire4);
    utils::Conv2dOptions fire5Options = fire4Options;
    const wnn::Operand fire5 = BuildBatchNormFire(builder, add1, 5, &fire5Options);
    const wnn::Operand add2 = builder.Add(add1, fire5);
    utils::Conv2dOptions fire6Options = fire4Options;
    const wnn::Operand fire6 = BuildBatchNormFire(builder, add2, 6, &fire6Options);
    utils::Conv2dOptions fire7Options;
    fire7Options.padding = padding;
    fire7Options.groups = 384;
    const wnn::Operand fire7 = BuildBatchNormFire(builder, fire6, 7, &fire7Options);
    const wnn::Operand add3 = builder.Add(fire6, fire7);
    utils::Conv2dOptions fire8Options = fire7Options;
    const wnn::Operand fire8 = BuildBatchNormFire(builder, add3, 8, &fire8Options);
    const wnn::Operand add4 = builder.Add(add3, fire8);
    utils::Conv2dOptions fire9Options = fire7Options;
    const wnn::Operand fire9 = BuildBatchNormFire(builder, add4, 9, &fire9Options);
    const wnn::Operand add5 = builder.Add(add4, fire9);
    utils::Conv2dOptions fire10Options = conv0Options;
    fire10Options.groups = 384;
    const wnn::Operand fire10 = BuildBatchNormFire(builder, add5, 10, &fire10Options);
    utils::Conv2dOptions fire11Options;
    fire11Options.padding = padding;
    fire11Options.groups = 576;
    const wnn::Operand fire11 = BuildBatchNormFire(builder, fire10, 11, &fire11Options);
    const wnn::Operand add6 = builder.Add(fire10, fire11);
    utils::Conv2dOptions fire12Options = fire11Options;
    const wnn::Operand fire12 = BuildBatchNormFire(builder, add6, 12, &fire12Options);
    const wnn::Operand add7 = builder.Add(add6, fire12);
    utils::Conv2dOptions fire13Options = conv0Options;
    fire13Options.groups = 576;
    const wnn::Operand fire13 = BuildBatchNormFire(builder, add7, 13, &fire13Options);
    utils::Conv2dOptions fire14Options;
    fire14Options.padding = padding;
    fire14Options.groups = 960;
    const wnn::Operand fire14 = BuildBatchNormFire(builder, fire13, 14, &fire14Options);
    const wnn::Operand add8 = builder.Add(fire13, fire14);
    utils::Conv2dOptions fire15Options = fire14Options;
    const wnn::Operand fire15 = BuildBatchNormFire(builder, add8, 15, &fire15Options);
    const wnn::Operand add9 = builder.Add(add8, fire15);
    utils::Conv2dOptions fire16Options = fire14Options;
    const wnn::Operand fire16 = BuildBatchNormFire(builder, add9, 16, &fire16Options);
    const wnn::Operand batchNorm1 = BuildConvBatchNorm(builder, fire16, 1, true);
    const wnn::Operand pool0 = builder.AveragePool2d(batchNorm1);
    const wnn::Operand convWeights1 =
        BuildConstantFromNpy(builder, mWeightsPath + "mobilenetv20_output_pred_weight.npy");
    const wnn::Operand conv1 = builder.Conv2d(pool0, convWeights1);
    const std::vector<int32_t> newShape = {1, -1};
    const wnn::Operand reshape0 = builder.Reshape(conv1, newShape.data(), newShape.size());
    const wnn::Operand output = softmax ? builder.Softmax(reshape0) : reshape0;
    return output;
}
