//* Copyright 2019 The Dawn Authors
//* Copyright 2021 The WebNN-native Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

#include "common/Log.h"
#include "webnn_wire/client/ApiObjects.h"
#include "webnn_wire/client/Client.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace webnn_wire::client {
    namespace {

        //* Outputs an rvalue that's the number of elements a pointer member points to.
        {% macro member_length(member, accessor) -%}
            {%- if member.length == "constant" -%}
                {{member.constant_length}}
            {%- else -%}
                {{accessor}}{{as_varName(member.length.name)}}
            {%- endif -%}
        {%- endmacro %}

        {% for type in by_category["object"] %}
            DAWN_DECLARE_UNUSED bool ClientMatches(const Client* client, const {{as_cType(type.name)}} obj) {
                return client == reinterpret_cast<const {{as_wireType(type)}}>(obj)->client;
            }

            DAWN_DECLARE_UNUSED bool ClientMatches(const Client* client, const {{as_cType(type.name)}} *const obj, uint32_t count = 1) {
                ASSERT(count == 0 || obj != nullptr);
                for (uint32_t i = 0; i < count; ++i) {
                    if (!ClientMatches(client, obj[i])) {
                        return false;
                    }
                }
                return true;
            }
        {% endfor %}

        {% for type in by_category["structure"] if type.may_have_dawn_object %}
            DAWN_DECLARE_UNUSED bool ClientMatches(const Client* client, const {{as_cType(type.name)}}& obj) {
                {% if type.extensible %}
                    if (!ClientMatches(client, obj.nextInChain)) {
                        return false;
                    }
                {% endif %}
                {% for member in type.members if member.type.may_have_dawn_object or member.type.category == "object" %}
                    {% if member.optional %}
                        if (obj.{{as_varName(member.name)}} != nullptr)
                    {% endif %}
                    {
                        if (!ClientMatches(client, obj.{{as_varName(member.name)}}
                            {%- if member.annotation != "value" and member.length != "strlen" -%}
                                , {{member_length(member, "obj.")}}
                            {%- endif -%})) {
                            return false;
                        }
                    }
                {% endfor %}
                return true;
            }

            DAWN_DECLARE_UNUSED bool ClientMatches(const Client* client, const {{as_cType(type.name)}} *const obj, uint32_t count = 1) {
                for (uint32_t i = 0; i < count; ++i) {
                    if (!ClientMatches(client, obj[i])) {
                        return false;
                    }
                }
                return true;
            }
        {% endfor %}

    }  // anonymous namespace

    //* Implementation of the client API functions.
    {% for type in by_category["object"] %}
        {% set Type = type.name.CamelCase() %}
        {% set cType = as_cType(type.name) %}

        {% for method in type.methods %}
            {% set Suffix = as_MethodSuffix(type.name, method.name) %}

            {% if Suffix in client_handwritten_commands %}
                static
            {% endif %}
            {{as_cType(method.return_type.name)}} Client{{Suffix}}(
                {{-cType}} cSelf
                {%- for arg in method.arguments -%}
                    , {{as_annotated_cType(arg)}}
                {%- endfor -%}
            ) {
                auto self = reinterpret_cast<{{as_wireType(type)}}>(cSelf);
                {% if len(method.arguments) > 0 %}
                    {
                        bool sameClient = true;
                        Client* client = self->client;
                        DAWN_UNUSED(client);

                        do {
                            {% for arg in method.arguments if arg.type.may_have_dawn_object or arg.type.category == "object" %}
                                {% if arg.optional %}
                                    if ({{as_varName(arg.name)}} != nullptr)
                                {% endif %}
                                {
                                    if (!ClientMatches(client, {{as_varName(arg.name)}}
                                        {%- if arg.annotation != "value" and arg.length != "strlen" -%}
                                            , {{member_length(arg, "")}}
                                        {%- endif -%})) {
                                        sameClient = false;
                                        break;
                                    }
                                }
                            {% endfor %}
                        } while (false);

                        if (DAWN_UNLIKELY(!sameClient)) {
                            // reinterpret_cast<Device*>(client->GetDevice())->InjectError(WGPUErrorType_Validation,
                            //                     "All objects must be from the same device.");
                            {% if method.return_type.category == "object" %}
                                // Allocate an object without registering it on the server. This is backed by a real allocation on
                                // the client so commands can be sent with it. But because it's not allocated on the server, it will
                                // be a fatal error to use it.
                                auto* allocation = self->client->{{method.return_type.name.CamelCase()}}Allocator().New(self->client);
                                return reinterpret_cast<{{as_cType(method.return_type.name)}}>(allocation->object.get());
                            {% elif method.return_type.name.canonical_case() == "void" %}
                                return;
                            {% else %}
                                return {};
                            {% endif %}
                        }
                    }
                {% endif %}

                {% if Suffix not in client_handwritten_commands %}
                    {{Suffix}}Cmd cmd;

                    //* Create the structure going on the wire on the stack and fill it with the value
                    //* arguments so it can compute its size.
                    cmd.self = cSelf;

                    //* For object creation, store the object ID the client will use for the result.
                    {% if method.return_type.category == "object" %}
                        auto* allocation = self->client->{{method.return_type.name.CamelCase()}}Allocator().New(self->client);
                        cmd.result = ObjectHandle{allocation->object->id, allocation->generation};
                    {% endif %}

                    {% for arg in method.arguments %}
                        cmd.{{as_varName(arg.name)}} = {{as_varName(arg.name)}};
                    {% endfor %}

                    //* Allocate space to send the command and copy the value args over.
                    self->client->SerializeCommand(cmd);

                    {% if method.return_type.category == "object" %}
                        return reinterpret_cast<{{as_cType(method.return_type.name)}}>(allocation->object.get());
                    {% endif %}
                {% else %}
                    return self->{{method.name.CamelCase()}}(
                        {%- for arg in method.arguments -%}
                            {%if not loop.first %}, {% endif %} {{as_varName(arg.name)}}
                        {%- endfor -%});
                {% endif %}
            }
        {% endfor %}

        //* When an object's refcount reaches 0, notify the server side of it and delete it.
        void Client{{as_MethodSuffix(type.name, Name("release"))}}({{cType}} cObj) {
            {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
            obj->refcount --;

            if (obj->refcount > 0) {
                return;
            }

            DestroyObjectCmd cmd;
            cmd.objectType = ObjectType::{{type.name.CamelCase()}};
            cmd.objectId = obj->id;

            obj->client->SerializeCommand(cmd);
            obj->client->{{type.name.CamelCase()}}Allocator().Free(obj);
        }

        void Client{{as_MethodSuffix(type.name, Name("reference"))}}({{cType}} cObj) {
            {{Type}}* obj = reinterpret_cast<{{Type}}*>(cObj);
            obj->refcount ++;
        }
    {% endfor %}

    namespace {

        struct ProcEntry {
            WebnnProc proc;
            const char* name;
        };
        static const ProcEntry sProcMap[] = {
            {% for (type, method) in c_methods_sorted_by_name %}
                { reinterpret_cast<WebnnProc>(Client{{as_MethodSuffix(type.name, method.name)}}), "{{as_cMethod(type.name, method.name)}}" },
            {% endfor %}
        };
        static constexpr size_t sProcMapSize = sizeof(sProcMap) / sizeof(sProcMap[0]);
    }  // anonymous namespace

    std::vector<const char*> GetProcMapNamesForTesting() {
        std::vector<const char*> result;
        result.reserve(sProcMapSize);
        for (const ProcEntry& entry : sProcMap) {
            result.push_back(entry.name);
        }
        return result;
    }

    WNNGraphBuilder ClientCreateGraphBuilder(WNNContext context) {
        auto self = reinterpret_cast<Context*>(context);
        CreateGraphBuilderCmd cmd;

        auto* allocation = self->client->GraphBuilderAllocator().New(self->client);
        cmd.result = ObjectHandle{allocation->object->id, allocation->generation};

        cmd.context = self->id;

        self->client->SerializeCommand(cmd);

        return reinterpret_cast<WNNGraphBuilder>(allocation->object.get());
    }

    WNNNamedInputs ClientCreateNamedInputs() {
        UNREACHABLE();
        return nullptr;
    }

    WNNNamedOperands ClientCreateNamedOperands() {
        UNREACHABLE();
        return nullptr;
    }

    WNNNamedOutputs ClientCreateNamedOutputs() {
        UNREACHABLE();
        return nullptr;
    }

    WNNOperatorArray ClientCreateOperatorArray() {
        UNREACHABLE();
        return nullptr;
    }

    static WebnnProcTable gProcTable = {
        ClientCreateGraphBuilder,
        ClientCreateNamedInputs,
        ClientCreateNamedOperands,
        ClientCreateNamedOutputs,
        ClientCreateOperatorArray,
        {% for type in by_category["object"] %}
            {% for method in c_methods(type) %}
                Client{{as_MethodSuffix(type.name, method.name)}},
            {% endfor %}
        {% endfor %}
    };
    const WebnnProcTable& GetProcs() {
        return gProcTable;
    }
}  // namespace webnn_wire::client
