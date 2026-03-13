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

#include "Result.h"
#include "core/Compatibility.h"

#include <nlohmann/json.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

// -------------------------------------------------------------------------
/*!
    \class OCIDescriptor
    \brief Parsed OCI descriptor object.

    \see https://github.com/opencontainers/image-spec/blob/main/descriptor.md

*/
class OCIDescriptor
{
public:
    static LIBRALF_NS::Result<OCIDescriptor> parse(const nlohmann::json &json);

public:
    OCIDescriptor(std::string &&digest, std::string &&mediaType, int64_t size, std::vector<uint8_t> &&data,
                  std::vector<std::string> &&urls, std::map<std::string, std::string> &&annotations)
        : m_digest(std::move(digest))
        , m_mediaType(std::move(mediaType))
        , m_size(size)
        , m_data(std::move(data))
        , m_urls(std::move(urls))
        , m_annotations(std::move(annotations))
    {
    }

    ~OCIDescriptor() = default;

    OCIDescriptor(const OCIDescriptor &) = default;
    OCIDescriptor(OCIDescriptor &&) = default;

    OCIDescriptor &operator=(const OCIDescriptor &) = default;
    OCIDescriptor &operator=(OCIDescriptor &&) = default;

    const std::string &mediaType() const { return m_mediaType; }
    const std::string &digest() const { return m_digest; }
    uint64_t size() const { return m_size; }

    const std::vector<uint8_t> &data() const { return m_data; }

    const std::vector<std::string> &urls() const { return m_urls; }

    const std::map<std::string, std::string> &annotations() const { return m_annotations; }

    std::string toString() const;

private:
    std::string m_digest;
    std::string m_mediaType;
    uint64_t m_size;

    std::vector<uint8_t> m_data;
    std::vector<std::string> m_urls;
    std::map<std::string, std::string> m_annotations;
};
