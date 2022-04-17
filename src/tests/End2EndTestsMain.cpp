// Copyright 2017 The Dawn Authors
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

#include "tests/WebnnTest.h"

int main(int argc, char** argv) {
    std::string devicePreference = "default", powerPreference = "default";
    for (int i = 1; i < argc; ++i) {
        if (strcmp("-d", argv[i]) == 0 && i + 1 < argc) {
            devicePreference = argv[i + 1];
        }
        if (strcmp("-p", argv[i]) == 0 && i + 1 < argc) {
            powerPreference = argv[i + 1];
        }
    }
    const wnn::ContextOptions options =
        utils::CreateContextOptions(devicePreference, powerPreference);
    InitWebnnEnd2EndTestEnvironment(&options);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
