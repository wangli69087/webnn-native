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

#include "ML.h"

#include "Context.h"

Napi::FunctionReference node::ML::constructor;

namespace node {

    ML::ML(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ML>(info) {
    }

    Napi::Value ML::CreateContext(const Napi::CallbackInfo& info) {
        Napi::Object context = Context::constructor.New({});
        return context;
    }

    Napi::Object ML::Initialize(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);
        Napi::Function func = DefineClass(
            env, "ml", {StaticMethod("createContext", &ML::CreateContext, napi_enumerable)});
        constructor = Napi::Persistent(func);
        constructor.SuppressDestruct();
        exports.Set("ml", func);
        return exports;
    }

}  // namespace node
