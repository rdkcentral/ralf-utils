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

#include "Error.h"
#include "core/Compatibility.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>

#include <functional>
#include <memory>

/// Some std::unique_ptr wrappers around libxml2 objects to manage their allocation and free
using xmlDocUniquePtr = std::unique_ptr<xmlDoc, std::function<void(xmlDoc *_Nonnull)>>;
using xmlNodeUniquePtr = std::unique_ptr<xmlNode, std::function<void(xmlNode *_Nonnull)>>;
using xmlXPathContextUniquePtr = std::unique_ptr<xmlXPathContext, std::function<void(xmlXPathContext *_Nonnull)>>;
using xmlXPathObjectUniquePtr = std::unique_ptr<xmlXPathObject, std::function<void(xmlXPathObject *_Nonnull)>>;
using xmlOutputBufferUniquePtr = std::unique_ptr<xmlOutputBuffer, std::function<void(xmlOutputBuffer *_Nonnull)>>;

using xmlCharUniquePtr = std::unique_ptr<xmlChar, std::function<void(xmlChar *_Nonnull)>>;

// -----------------------------------------------------------------------------
/*!
    \internal

    Base64 decodes a \a base64 string to a vector of bytes.  If there was an
    error decoding the string then an empty vector is returned, and if \a error
    was not null then it is populated with the error details.

 */
std::vector<uint8_t> base64Decode(const std::string &base64, LIBRALF_NS::Error *_Nullable error);
std::vector<uint8_t> base64Decode(const xmlChar *_Nonnull base64, size_t length, LIBRALF_NS::Error *_Nullable error);

// -----------------------------------------------------------------------------
/*!
    \internal

    Helper function to do an XPath evaluation on the given \a doc using the
    \a expression.  If there was an error evaluating the expression then nullptr
    is returned, and if \a error was not null then it is populated with the error
    reason.

 */
xmlXPathObjectUniquePtr evaluateXPath(xmlDoc *_Nonnull doc, const std::string &expression,
                                      LIBRALF_NS::Error *_Nullable error);
