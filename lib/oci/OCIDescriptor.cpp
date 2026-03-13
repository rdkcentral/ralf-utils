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

#include "OCIDescriptor.h"
#include "OCIUtils.h"

#include "core/Base64.h"
#include "core/LogMacros.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -------------------------------------------------------------------------
/*!
    \static

    Process the parsed OCI descriptor JSON object and return a new OCIDescriptor
    object.

    A typical descriptor looks like this:
    \code
        {
            "mediaType": "application/vnd.oci.image.manifest.v1+json",
            "digest": "sha256:4161227e2a40097d5e00150b6027e86ea78a03f3668d41cf240173dc9b199614",
            "size": 469,
            "annotations": {
                "org.opencontainers.image.ref.name": "test"
            }
        }
    \endcode

    \see https://github.com/opencontainers/image-spec/blob/main/descriptor.md
*/
Result<OCIDescriptor> OCIDescriptor::parse(const nlohmann::json &json)
{
    std::string mediaType;
    std::string digest;
    int64_t size = -1;
    std::vector<uint8_t> data;
    std::vector<std::string> urls;
    std::map<std::string, std::string> annotations;

    // Check the manifest is an object
    if (!json.is_object())
        return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - not a JSON object");

    for (const auto &[key, value] : json.items())
    {
        if (key == "mediaType")
        {
            if (!value.is_string())
                return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - mediaType is not a string");

            mediaType = value.get<std::string>();
        }
        else if (key == "digest")
        {
            if (!value.is_string())
                return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - digest is not a string");

            digest = value.get<std::string>();
        }
        else if (key == "size")
        {
            if (!value.is_number_integer())
                return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - size is not an integer");

            size = value.get<int64_t>();
        }
        else if (key == "data")
        {
            if (!value.is_string())
                return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - data is not a string");

            auto result = Base64::decode(value.get<std::string>());
            if (!result)
                return Error(ErrorCode::PackageContentsInvalid,
                             "Invalid OCI descriptor - data is not a valid base64 string");

            data = std::move(result.value());
        }
        else if (key == "urls")
        {
            if (!value.is_array())
                return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - urls is not a JSON array");

            for (const auto &url : value)
            {
                if (!url.is_string())
                    return Error(ErrorCode::PackageContentsInvalid,
                                 "Invalid OCI descriptor - url value is not a string");

                urls.push_back(url.get<std::string>());
            }
        }
        else if (key == "annotations")
        {
            if (!value.is_object())
                return Error(ErrorCode::PackageContentsInvalid,
                             "Invalid OCI descriptor - annotations is not a JSON object");

            for (const auto &[annotationKey, annotationValue] : value.items())
            {
                if (!annotationValue.is_string())
                    return Error(ErrorCode::PackageContentsInvalid,
                                 "Invalid OCI descriptor - annotation value is not a string");

                annotations[annotationKey] = annotationValue.get<std::string>();
            }
        }
    }

    auto checkedDigest = validateDigest(digest);
    if (!checkedDigest)
        return Error::format(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - invalid digest '%s'",
                             digest.c_str());

    if (size < 0)
        return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - invalid size");
    else if (size > INT32_MAX)
        return Error(ErrorCode::PackageContentsInvalid, "Invalid OCI descriptor - size is too large");

    return OCIDescriptor(std::move(checkedDigest.value()), std::move(mediaType), size, std::move(data), std::move(urls),
                         std::move(annotations));
}

// -------------------------------------------------------------------------
/*!
    Debugging util to convert the descriptor back to a JSON string.

 */
std::string OCIDescriptor::toString() const
{
    nlohmann::json obj = { { "mediaType", m_mediaType }, { "digest", "sha256:" + m_digest }, { "size", m_size } };

    if (!m_urls.empty())
        obj["urls"] = m_urls;

    if (!m_annotations.empty())
        obj["annotations"] = m_annotations;

    if (!m_data.empty())
    {
        auto b64 = Base64::encode(m_data);
        if (b64)
            obj["data"] = b64.value();
    }

    return obj.dump(2);
}