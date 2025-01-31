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

#ifndef WEBNN_NATIVE_DML_MODEL_DML_H_
#define WEBNN_NATIVE_DML_MODEL_DML_H_

#define DML_TARGET_VERSION_USE_LATEST 1

#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <wrl\client.h>
#include <unordered_map>
#include <unordered_set>

#include "DirectML.h"
#include "webnn/native/Graph.h"
#include "webnn/native/Operand.h"
#include "webnn/native/Operator.h"
#include "webnn/native/dml/ContextDML.h"
#include "webnn/native/ops/BatchNorm.h"
#include "webnn/native/ops/Binary.h"
#include "webnn/native/ops/Clamp.h"
#include "webnn/native/ops/Concat.h"
#include "webnn/native/ops/Constant.h"
#include "webnn/native/ops/Conv2d.h"
#include "webnn/native/ops/Gemm.h"
#include "webnn/native/ops/Gru.h"
#include "webnn/native/ops/Input.h"
#include "webnn/native/ops/InstanceNorm.h"
#include "webnn/native/ops/LeakyRelu.h"
#include "webnn/native/ops/Pad.h"
#include "webnn/native/ops/Pool2d.h"
#include "webnn/native/ops/Reduce.h"
#include "webnn/native/ops/Resample2d.h"
#include "webnn/native/ops/Reshape.h"
#include "webnn/native/ops/Slice.h"
#include "webnn/native/ops/Split.h"
#include "webnn/native/ops/Squeeze.h"
#include "webnn/native/ops/Transpose.h"
#include "webnn/native/ops/Unary.h"
namespace webnn::native { namespace dml {

    using namespace Microsoft::WRL;

    // Represent the DirectML tensor description.
    struct DmlTensorDesc {
        std::vector<UINT> dimensions = {};
        std::vector<UINT> strides = {};
        // Describes a tensor that will be stored in a Direct3D 12 buffer resource.
        DML_BUFFER_TENSOR_DESC bufferDesc = {};
    };

    // Represent the information of the graph's edges.
    struct EdgeInfoBase {
        virtual ~EdgeInfoBase() = default;
        DML_TENSOR_DESC outputTensorDESC = {};
        std::string name = "";
        bool isInputEdge = false;
    };

    // Only represent the information of the input edges.
    struct InputEdgeInfo final : public EdgeInfoBase {
        ~InputEdgeInfo() override = default;
        // Indicate the index of the graph's input.
        size_t inputIndex = 0;
        void const* buffer = nullptr;
        size_t byteLength = 0;
        // Indicate if the input is from constant buffer which need to be
        // uploaded in the stage of initialization.
        bool isConstantInput = false;
    };

    // Represent the information of the intermediate edges and output edges.
    struct EdgeInfo final : public EdgeInfoBase {
        ~EdgeInfo() override = default;
        // Indicate the index of the intermediate node from which this edge was produced.
        uint32_t nodeIndex = 0;
        // Indicate the index of the intermediate node' output from which this edge was produced.
        uint32_t outputNodeIndex = 0;
    };

    class Graph : public GraphBase {
      public:
        explicit Graph(Context* context);
        ~Graph() override = default;

        virtual MaybeError AddConstant(const op::Constant* constant) override;
        virtual MaybeError AddInput(const op::Input* input) override;
        virtual MaybeError AddOutput(std::string_view name, const OperandBase* output) override;
        virtual MaybeError AddBatchNorm(const op::BatchNorm* batchNorm) override;
        virtual MaybeError AddBinary(const op::Binary* binary) override;
        virtual MaybeError AddConv2d(const op::Conv2d* conv2d) override;
        virtual MaybeError AddConvTranspose2d(const op::ConvTranspose2d* convTranspose2d) override;
        virtual MaybeError AddPad(const op::Pad* pad) override;
        virtual MaybeError AddPool2d(const op::Pool2d* pool2d) override;
        virtual MaybeError AddReduce(const op::Reduce* reduce) override;
        virtual MaybeError AddResample2d(const op::Resample2d* resample2d) override;
        virtual MaybeError AddReshape(const op::Reshape* reshape) override;
        virtual MaybeError AddSlice(const op::Slice* slice) override;
        virtual MaybeError AddSplit(const op::Split* split) override;
        virtual MaybeError AddSqueeze(const op::Squeeze* squeeze) override;
        virtual MaybeError AddTranspose(const op::Transpose* transpose) override;
        virtual MaybeError AddUnary(const op::Unary* unary) override;
        virtual MaybeError AddGemm(const op::Gemm* Gemm) override;
        virtual MaybeError AddGru(const op::Gru* Gru) override;
        virtual MaybeError AddConcat(const op::Concat* concat) override;
        virtual MaybeError AddClamp(const op::Clamp* clamp) override;
        virtual MaybeError AddInstanceNorm(const op::InstanceNorm* instanceNorm) override;
        virtual MaybeError Finish() override;

        void AddEdgesToThisNode(std::vector<std::shared_ptr<EdgeInfoBase>> inputNodes);
        void FillUploadResourceAndInputBindings(
            uint64_t uploadResourceSize,
            std::vector<DML_BUFFER_BINDING>& inputBufferBinding,
            std::unordered_map<std::string, Input> namedInputs = {});
        MaybeError createConstantInput(DML_TENSOR_DESC& inputTensorDESC,
                                       void const* value,
                                       size_t size,
                                       const std::vector<UINT>& dmlTensorDims,
                                       const std::vector<UINT>& strides = {},
                                       DML_TENSOR_DATA_TYPE dataType = DML_TENSOR_DATA_TYPE_FLOAT32,
                                       DML_TENSOR_FLAGS tensorFlag = DML_TENSOR_FLAG_OWNED_BY_DML);
        std::shared_ptr<EdgeInfoBase> Clamp(const op::ClampBase* clamp,
                                            std::shared_ptr<EdgeInfoBase> inputEdge);
        MaybeError HardSwish(std::shared_ptr<EdgeInfoBase>& inputEdge,
                             const std::vector<UINT>& inputDims);
        MaybeError EmulateFusedOperator(FusionOperatorBase* activation,
                                        std::shared_ptr<EdgeInfoBase>& inputEdge,
                                        const std::vector<UINT>& inputDims);
        MaybeError TransposeOutputToNhwc(std::shared_ptr<EdgeInfoBase>& inputEdge,
                                         const std::vector<UINT>& nchwOutputDims);

      private:
        MaybeError CompileImpl() override;
        MaybeError ComputeImpl(NamedInputsBase* inputs, NamedOutputsBase* outputs) override;

        // Represents a DirectML device, which is used to create operators, binding tables, command
        // recorders, and other objects.
        ComPtr<IDMLDevice> mDevice;
        // The IDMLDevice1 interface inherits from IDMLDevice.
        ComPtr<IDMLDevice1> mDevice1;
        // Represents a virtual adapter; it is used to create command allocators, command lists,
        // command queues, fences, resources, pipeline state objects, heaps, root signatures,
        // samplers, and many resource views.
        ComPtr<ID3D12Device> mD3D12Device;

        ComPtr<IDMLCommandRecorder> mCommandRecorder;
        ComPtr<ID3D12CommandQueue> mCommandQueue;
        ComPtr<ID3D12CommandAllocator> mCommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> mCommandList;
        ComPtr<IDMLBindingTable> mBindingTable;
        ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;

        ComPtr<ID3D12Resource> mUploadResource;
        ComPtr<ID3D12Resource> mInputResource;
        ComPtr<ID3D12Resource> mOutputResource;
        ComPtr<ID3D12Resource> mReadBackResource;
        ComPtr<ID3D12Resource> mTemporaryResource;
        ComPtr<ID3D12Resource> mPersistentResource;

        uint64_t mCommonInputsResourceSize = 0;
        uint64_t mOutputsResourceSize = 0;
        UINT64 mTemporaryResourceSize = 0;
        UINT64 mPersistentResourceSize = 0;

        // Describe a graph of DirectML operators used to compile a combined, optimized operator.
        std::vector<std::shared_ptr<InputEdgeInfo>> mInputs;
        std::vector<EdgeInfo> mOutputs;
        std::vector<DML_GRAPH_NODE_DESC> mIntermediateNodes;
        std::vector<DML_GRAPH_EDGE_DESC> mInputEdges;
        std::vector<DML_GRAPH_EDGE_DESC> mOutputEdges;
        std::vector<DML_GRAPH_EDGE_DESC> mIntermediateEdges;

        // IDMLCompiledOperator represents the DirectML graph's output which need to be initialized
        // by IDMLOperatorInitializer.
        ComPtr<IDMLCompiledOperator> mCompiledOperator;
        DML_BINDING_TABLE_DESC mBindingTableDesc;

        std::map<const OperandBase*, std::shared_ptr<EdgeInfoBase>> mGraphEdgesMap;

        // Keep intermediate nodes here to avoid releasing too early.
        std::map<uint32_t, ComPtr<IDMLOperator>> mIntermediateNodesMap;
        // Keep the input tensors description here to avoid releasing too early.
        std::vector<std::shared_ptr<DmlTensorDesc>> mDmlTensorsDesc;
        // Keep the descriptions of nodes and edges here to avoid releasing too early.
        std::vector<std::unique_ptr<DML_OPERATOR_GRAPH_NODE_DESC>> mIntermediateNodesDesc;
        std::vector<std::unique_ptr<DML_INPUT_GRAPH_EDGE_DESC>> mInputEdgesDesc;
        std::vector<std::unique_ptr<DML_OUTPUT_GRAPH_EDGE_DESC>> mOutputEdgesDesc;
        std::vector<std::unique_ptr<DML_INTERMEDIATE_GRAPH_EDGE_DESC>> mIntermediateEdgesDesc;
        std::unordered_set<const OperandBase*> mConstantSet;
        std::vector<std::unique_ptr<char>> mConstantsBuffer;
    };

}}  // namespace webnn::native::dml

#endif  // WEBNN_NATIVE_DML_MODEL_DML_H_