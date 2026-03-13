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

#include "Compatibility.h"
#include "Error.h"
#include "LibRalf.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlschemas.h>

#include <cstdarg>
#include <mutex>

class XmlSchemaValidator
{
public:
    explicit XmlSchemaValidator(const std::string &schema);
    XmlSchemaValidator(const char *_Nonnull schema, size_t length);
    ~XmlSchemaValidator();

    bool isValid() const;

    bool validate(xmlDoc *_Nonnull doc, LIBRALF_NS::Error *_Nullable error) const;

private:
    bool load(const char *_Nonnull schema, size_t length);

    static void onSchemaError(void *_Nullable context, const char *_Nullable message, ...);

    static std::string getLastLibxml2Error();

private:
    xmlDocPtr _Nullable m_schemaXml = nullptr;
    xmlSchemaParserCtxtPtr _Nullable m_parserContext = nullptr;
    xmlSchemaPtr _Nullable m_schema = nullptr;

    mutable std::mutex m_lock;
};
