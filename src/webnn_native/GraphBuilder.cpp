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

#include "webnn_native/GraphBuilder.h"

#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

#include "common/Assert.h"
#include "common/Log.h"
#include "common/RefCounted.h"
#include "webnn_native/Context.h"
#include "webnn_native/Graph.h"
#include "webnn_native/Operand.h"
#include "webnn_native/OperandArray.h"
#include "webnn_native/Operator.h"
#include "webnn_native/ops/BatchNorm.h"
#include "webnn_native/ops/Binary.h"
#include "webnn_native/ops/Clamp.h"
#include "webnn_native/ops/Concat.h"
#include "webnn_native/ops/Constant.h"
#include "webnn_native/ops/Conv2d.h"
#include "webnn_native/ops/Gemm.h"
#include "webnn_native/ops/Gru.h"
#include "webnn_native/ops/Input.h"
#include "webnn_native/ops/InstanceNorm.h"
#include "webnn_native/ops/LeakyRelu.h"
#include "webnn_native/ops/Pad.h"
#include "webnn_native/ops/Pool2d.h"
#include "webnn_native/ops/Reduce.h"
#include "webnn_native/ops/Resample2d.h"
#include "webnn_native/ops/Reshape.h"
#include "webnn_native/ops/Slice.h"
#include "webnn_native/ops/Split.h"
#include "webnn_native/ops/Squeeze.h"
#include "webnn_native/ops/Transpose.h"
#include "webnn_native/ops/Unary.h"

#define WEBNN_VALIDATE(ptr, objectBase)                                  \
    Ref<OperatorBase> op = AcquireRef(ptr);                              \
    if (GetContext()->ConsumedError(op->ValidateAndInferOutputInfo())) { \
        return objectBase::MakeError(this);                              \
    }                                                                    \
    for (;;)                                                             \
    break

#define VALIDATE_FOR_OPERAND(ptr)     \
    WEBNN_VALIDATE(ptr, OperandBase); \
    return op->PrimaryOutput()
#define VALIDATE_ARRAY_OPERAND(ptr)        \
    WEBNN_VALIDATE(ptr, OperandArrayBase); \
    return new OperandArrayBase(this, op->Outputs())

namespace webnn_native {

    GraphBuilderBase::GraphBuilderBase(ContextBase* context) : ObjectBase(context) {
    }

    OperandBase* GraphBuilderBase::Abs(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kAbs, input));
    }

    OperandBase* GraphBuilderBase::Add(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kAdd, a, b));
    }

    OperandBase* GraphBuilderBase::AveragePool2d(OperandBase* input, Pool2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Pool2d(this, op::Pool2dType::kAveragePool2d, input, options));
    }

    OperandBase* GraphBuilderBase::BatchNorm(OperandBase* input,
                                             OperandBase* mean,
                                             OperandBase* variance,
                                             BatchNormOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::BatchNorm(this, input, mean, variance, options));
    }

    OperandBase* GraphBuilderBase::Clamp(OperandBase* input, ClampOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Clamp(this, input, options));
    }

    FusionOperatorBase* GraphBuilderBase::ClampOperator(ClampOptions const* options) {
        return new op::FusionClamp(this, options);
    }

    OperandBase* GraphBuilderBase::Ceil(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kCeil, input));
    }

    OperandBase* GraphBuilderBase::Concat(uint32_t inputsCount,
                                          OperandBase* const* inputs,
                                          uint32_t axis) {
        std::vector<Ref<OperandBase>> operandInputs;
        operandInputs.reserve(inputsCount);
        for (uint32_t i = 0; i < inputsCount; ++i) {
            operandInputs.push_back(inputs[i]);
        }
        VALIDATE_FOR_OPERAND(new op::Concat(this, std::move(operandInputs), axis));
    }

    OperandBase* GraphBuilderBase::Constant(OperandDescriptor const* desc,
                                            ArrayBufferView const* arrayBuffer) {
        VALIDATE_FOR_OPERAND(new op::Constant(this, desc, arrayBuffer));
    }

    OperandBase* GraphBuilderBase::ConstantWithGpuBuffer(OperandDescriptor const* desc,
                                                         GpuBufferView const* gpuBuffer) {
#if defined(WEBNN_ENABLE_GPU_BUFFER)
        VALIDATE_FOR_OPERAND(new op::Constant(this, desc, gpuBuffer));
#endif
        UNREACHABLE();
        return nullptr;
    }

    OperandBase* GraphBuilderBase::Conv2d(OperandBase* input,
                                          OperandBase* filter,
                                          Conv2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Conv2d(this, input, filter, options));
    }

    OperandBase* GraphBuilderBase::ConvTranspose2d(OperandBase* input,
                                                   OperandBase* filter,
                                                   ConvTranspose2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::ConvTranspose2d(this, input, filter, options));
    }

    OperandBase* GraphBuilderBase::Cos(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kCos, input));
    }

    OperandBase* GraphBuilderBase::Div(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kDiv, a, b));
    }

    OperandBase* GraphBuilderBase::Exp(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kExp, input));
    }

    OperandBase* GraphBuilderBase::Floor(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kFloor, input));
    }

    OperandBase* GraphBuilderBase::Gemm(OperandBase* a,
                                        OperandBase* b,
                                        GemmOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Gemm(this, a, b, options));
    }

    OperandArrayBase* GraphBuilderBase::Gru(OperandBase* input,
                                            OperandBase* weight,
                                            OperandBase* recurrentWeight,
                                            int32_t steps,
                                            int32_t hiddenSize,
                                            GruOptions const* options) {
        VALIDATE_ARRAY_OPERAND(
            new op::Gru(this, input, weight, recurrentWeight, steps, hiddenSize, options));
    }

    OperandBase* GraphBuilderBase::HardSwish(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kHardSwish, input));
    }

    FusionOperatorBase* GraphBuilderBase::HardSwishOperator() {
        return new op::FusionUnary(this, FusionType::HardSwish);
    }

    OperandBase* GraphBuilderBase::Input(char const* name, OperandDescriptor const* desc) {
        VALIDATE_FOR_OPERAND(new op::Input(this, std::string(name), desc));
    }

    OperandBase* GraphBuilderBase::InstanceNorm(OperandBase* input,
                                                InstanceNormOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::InstanceNorm(this, input, options));
    }

    OperandBase* GraphBuilderBase::LeakyRelu(OperandBase* input, LeakyReluOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::LeakyRelu(this, input, options));
    }

    FusionOperatorBase* GraphBuilderBase::LeakyReluOperator(LeakyReluOptions const* options) {
        return new op::FusionLeakyRelu(this, options);
    }

    OperandBase* GraphBuilderBase::Log(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kLog, input));
    }

    OperandBase* GraphBuilderBase::L2Pool2d(OperandBase* input, Pool2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Pool2d(this, op::Pool2dType::kL2Pool2d, input, options));
    }

    OperandBase* GraphBuilderBase::Matmul(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kMatMul, a, b));
    }

    OperandBase* GraphBuilderBase::Max(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kMax, a, b));
    }

    OperandBase* GraphBuilderBase::MaxPool2d(OperandBase* input, Pool2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Pool2d(this, op::Pool2dType::kMaxPool2d, input, options));
    }

    OperandBase* GraphBuilderBase::Min(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kMin, a, b));
    }

    OperandBase* GraphBuilderBase::Mul(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kMul, a, b));
    }

    OperandBase* GraphBuilderBase::Neg(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kNeg, input));
    }

    // OperandBase* GraphBuilderBase::Pad(OperandBase* input,
    //                                    uint32_t const* padding,
    //                                    size_t padding_count,
    //                                    PadOptions const* options) {
    //     VALIDATE_FOR_OPERAND(new op::Pad(this, input, padding, padding_count, options));
    // }
    OperandBase* GraphBuilderBase::Pad(OperandBase* input,
                                       OperandBase* padding,
                                       PadOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Pad(this, input, padding, options));
    }

    OperandBase* GraphBuilderBase::Pow(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kPower, a, b));
    }

    OperandBase* GraphBuilderBase::ReduceArgMax(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceArgMax, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceArgMin(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceArgMin, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceL2(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceL2, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceL1(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceL1, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceMax(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceMax, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceMean(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceMean, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceMin(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceMin, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceProduct(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceProduct, input, options));
    }

    OperandBase* GraphBuilderBase::ReduceSum(OperandBase* input, ReduceOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Reduce(this, op::ReduceType::kReduceSum, input, options));
    }

    OperandBase* GraphBuilderBase::Relu(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kRelu, input));
    }

    FusionOperatorBase* GraphBuilderBase::ReluOperator() {
        return new op::FusionUnary(this, FusionType::Relu);
    }

    OperandBase* GraphBuilderBase::Resample2d(OperandBase* input,
                                              Resample2dOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Resample2d(this, input, options));
    }

    OperandBase* GraphBuilderBase::Reshape(OperandBase* input,
                                           int32_t const* new_shape,
                                           size_t new_shape_count) {
        VALIDATE_FOR_OPERAND(new op::Reshape(this, input, new_shape, new_shape_count));
    }

    OperandBase* GraphBuilderBase::Sigmoid(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kSigmoid, input));
    }

    FusionOperatorBase* GraphBuilderBase::SigmoidOperator() {
        return new op::FusionUnary(this, FusionType::Sigmoid);
    }

    OperandBase* GraphBuilderBase::Sin(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kSin, input));
    }

    OperandBase* GraphBuilderBase::Slice(OperandBase* input,
                                         int32_t const* starts,
                                         uint32_t startsCount,
                                         int32_t const* sizes,
                                         uint32_t sizesCount,
                                         SliceOptions const* options) {
        VALIDATE_FOR_OPERAND(
            new op::Slice(this, input, starts, startsCount, sizes, sizesCount, options));
    }

    OperandBase* GraphBuilderBase::Softmax(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kSoftmax, input));
    }

    OperandArrayBase* GraphBuilderBase::Split(OperandBase* input,
                                              uint32_t const* splits,
                                              uint32_t splitsCount,
                                              SplitOptions const* options) {
        VALIDATE_ARRAY_OPERAND(new op::Split(this, input, splits, splitsCount, options));
    }

    OperandBase* GraphBuilderBase::Squeeze(OperandBase* input, SqueezeOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Squeeze(this, input, options));
    }

    OperandBase* GraphBuilderBase::Sub(OperandBase* a, OperandBase* b) {
        VALIDATE_FOR_OPERAND(new op::Binary(this, op::BinaryOpType::kSub, a, b));
    }

    OperandBase* GraphBuilderBase::Tan(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kTan, input));
    }

    OperandBase* GraphBuilderBase::Tanh(OperandBase* input) {
        VALIDATE_FOR_OPERAND(new op::Unary(this, op::UnaryOpType::kTanh, input));
    }

    FusionOperatorBase* GraphBuilderBase::TanhOperator() {
        return new op::FusionUnary(this, FusionType::Tanh);
    }

    OperandBase* GraphBuilderBase::Transpose(OperandBase* input, TransposeOptions const* options) {
        VALIDATE_FOR_OPERAND(new op::Transpose(this, input, options));
    }

    ResultOrError<Ref<GraphBase>> GraphBuilderBase::BuildImpl(
        NamedOperandsBase const* namedOperands) {
        DAWN_INVALID_IF(this->IsError(), "The GraphBuilderBase is an error object.");
        DAWN_INVALID_IF(namedOperands->GetRecords().empty(), "The namedOperands are empty.");

        std::vector<const OperandBase*> outputs;
        for (auto& namedOutput : namedOperands->GetRecords()) {
            outputs.push_back(namedOutput.second);
        }
        std::vector<const OperatorBase*> sorted_operands = TopologicalSort(outputs);
        DAWN_INVALID_IF(sorted_operands.empty(), "The graph can't be built.");
        Ref<GraphBase> graph = AcquireRef(GetContext()->CreateGraph());
        for (auto& op : sorted_operands) {
            DAWN_INVALID_IF(op->IsError(), "The operand is an error object.");
            DAWN_TRY(op->AddToGraph(graph.Get()));
        }
        for (auto& [name, output] : namedOperands->GetRecords()) {
            DAWN_TRY(graph->AddOutput(name, output));
        }
        DAWN_TRY(graph->Finish());
        DAWN_TRY(graph->Compile());

        return std::move(graph);
    }

    GraphBase* GraphBuilderBase::Build(NamedOperandsBase const* namedOperands) {
        Ref<GraphBase> result = nullptr;
        if (GetContext()->ConsumedError(BuildImpl(namedOperands), &result)) {
            ASSERT(result == nullptr);
            return GraphBase::MakeError(this->GetContext());
        }
        return result.Detach();
    }

    // The implementation derives from nGraph topological_sort in
    // https://github.com/openvinotoolkit/openvino/blob/master/ngraph/core/include/ngraph/graph_util.hpp
    //
    //*****************************************************************************
    // Copyright 2017-2020 Intel Corporation
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
    //*****************************************************************************
    std::vector<const OperatorBase*> GraphBuilderBase::TopologicalSort(
        std::vector<const OperandBase*>& rootNodes) {
        std::stack<const OperatorBase*> nodesToDo;
        std::unordered_set<const OperatorBase*> nodesDone;
        std::vector<const OperatorBase*> result;

        for (auto node : rootNodes) {
            if (node->IsError()) {
                return {};
            }
            nodesToDo.push(const_cast<OperandBase*>(node)->Operator());
        }
        while (nodesToDo.size() > 0) {
            const OperatorBase* node = nodesToDo.top();
            if (nodesDone.count(node) == 0) {
                bool can_add = true;
                for (auto& dep : node->Inputs()) {
                    if (nodesDone.count(dep->Operator()) == 0) {
                        can_add = false;
                        nodesToDo.push(dep->Operator());
                    }
                }
                if (can_add) {
                    result.push_back(node);
                    nodesToDo.pop();
                    nodesDone.insert(node);
                }
            } else {
                nodesToDo.pop();
            }
        }
        return result;
    }

}  // namespace webnn_native
