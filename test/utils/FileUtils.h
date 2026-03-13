/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <array>
#include <cinttypes>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

std::vector<uint8_t> fileContents(const std::filesystem::path &path, size_t offset = 0, ssize_t size = -1);

std::string fileStrContents(const std::filesystem::path &path, size_t offset = 0, ssize_t size = -1);

std::array<uint8_t, 32> fileSha256(const std::filesystem::path &path, size_t offset = 0, ssize_t size = -1);
