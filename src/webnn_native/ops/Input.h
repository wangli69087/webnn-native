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

#ifndef WEBNN_NATIVE_OPS_INPUT_H_
#define WEBNN_NATIVE_OPS_INPUT_H_

#include <memory>
#include <string>

#include "webnn_native/Graph.h"
#include "webnn_native/Operand.h"

namespace webnn_native::op {

    class Input final : public OperatorBase {
      public:
        Input(GraphBuilderBase* builder, const std::string& name, const OperandDescriptor* desc)
            : OperatorBase(builder), mName(name) {
            mDescriptor.type = desc->type;
            mDimensions.assign(desc->dimensions, desc->dimensions + desc->dimensionsCount);
            mDescriptor.dimensions = mDimensions.data();
            mDescriptor.dimensionsCount = mDimensions.size();
        }
        ~Input() override = default;

        MaybeError AddToGraph(GraphBase* graph) const override {
            return graph->AddInput(this);
        }

        MaybeError ValidateAndInferOutputInfo() override {
            mOutputs[0]->SetType(mDescriptor.type);
            mOutputs[0]->SetShape(mDimensions);
            return {};
        }

        const std::string& GetName() const {
            return mName;
        }

        const OperandDescriptor* GetOperandDescriptor() const {
            return &mDescriptor;
        }

      private:
        std::string mName;
        OperandDescriptor mDescriptor;
        std::vector<int32_t> mDimensions;
    };

}  // namespace webnn_native::op

#endif  // WEBNN_NATIVE_OPS_INPUT_H_
