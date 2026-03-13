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

#include "XmlDSigUtils.h"
#include "core/Base64.h"

#include <libxml/xpathInternals.h>

#include <algorithm>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

xmlXPathObjectUniquePtr evaluateXPath(xmlDoc *_Nonnull doc, const std::string &expression, Error *_Nullable error)
{
    auto context = xmlXPathContextUniquePtr(xmlXPathNewContext(doc), xmlXPathFreeContext);
    if (!context)
    {
        if (error)
            error->assign(ErrorCode::GenericXmlError, "Failed to create XPath context");

        return nullptr;
    }

    xmlXPathRegisterNs(context.get(), BAD_CAST "ds", BAD_CAST "http://www.w3.org/2000/09/xmldsig#");

    auto xpath =
        xmlXPathObjectUniquePtr(xmlXPathEvalExpression(BAD_CAST expression.c_str(), context.get()), xmlXPathFreeObject);
    if (!xpath)
    {
        if (error)
            error->assign(ErrorCode::GenericXmlError, "Failed to evaluate XPath expression");

        return nullptr;
    }

    return xpath;
}

std::vector<uint8_t> base64Decode(const xmlChar *base64, size_t length, Error *_Nullable error)
{
    return base64Decode(std::string(reinterpret_cast<const char *>(base64), length), error);
}

std::vector<uint8_t> base64Decode(const std::string &base64, Error *_Nullable error)
{
    auto result = Base64::decode(base64);
    if (!result)
    {
        if (error)
            error->assign(ErrorCode::GenericCryptoError, "Failed to decode base64 data");

        return {};
    }

    return result.value();
}
