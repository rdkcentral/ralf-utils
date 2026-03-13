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

#include "XmlSchemaValidator.h"
#include "LogMacros.h"

#include <cstring>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

XmlSchemaValidator::XmlSchemaValidator(const char *_Nonnull schema, size_t length)
{
    load(schema, length);
}

XmlSchemaValidator::XmlSchemaValidator(const std::string &schema)
{
    load(schema.data(), schema.size());
}

XmlSchemaValidator::~XmlSchemaValidator()
{
    if (m_schema)
        xmlSchemaFree(m_schema);
    if (m_parserContext)
        xmlSchemaFreeParserCtxt(m_parserContext);
    if (m_schemaXml)
        xmlFreeDoc(m_schemaXml);
}

std::string XmlSchemaValidator::getLastLibxml2Error()
{
    const auto *error = xmlGetLastError();
    if (error)
    {
        std::string errorMsg = error->message ? error->message : "Unknown error";
        return errorMsg;
    }
    return "No error";
}

bool XmlSchemaValidator::load(const char *_Nonnull schema, size_t length)
{
    m_schemaXml = xmlReadMemory(schema, static_cast<int>(length), "", "", 0);
    if (!m_schemaXml)
    {
        logError("Failed to parse schema XML - %s", getLastLibxml2Error().c_str());
        return false;
    }

    m_parserContext = xmlSchemaNewDocParserCtxt(m_schemaXml);
    if (!m_parserContext)
    {
        logError("Failed to create XML schema context - %s", getLastLibxml2Error().c_str());

        xmlFreeDoc(m_schemaXml);
        m_schemaXml = nullptr;

        return false;
    }

    m_schema = xmlSchemaParse(m_parserContext);
    if (!m_schema)
    {
        logError("Failed to parse XML to schema object - %s", getLastLibxml2Error().c_str());

        xmlSchemaFreeParserCtxt(m_parserContext);
        m_parserContext = nullptr;
        xmlFreeDoc(m_schemaXml);
        m_schemaXml = nullptr;

        return false;
    }

    return true;
}

bool XmlSchemaValidator::isValid() const
{
    return (m_schema != nullptr);
}

void XmlSchemaValidator::onSchemaError(void *context, const char *message, ...)
{
    auto *error = reinterpret_cast<Error *>(context);
    if (error)
    {
        va_list args;
        va_start(args, message);
        *error = Error::format(ErrorCode::XmlSchemaError, message, args);
        va_end(args);
    }
}

bool XmlSchemaValidator::validate(xmlDoc *_Nonnull doc, Error *_Nullable error) const
{
    if (!m_schema)
    {
        if (error)
            error->assign(ErrorCode::InternalError, "Failed to initialise schema validator");
        return false;
    }

    // I don't know if libxml2 is thread-safe, but for caution we wrap the validation in a lock
    std::lock_guard lock(m_lock);

    auto *context = xmlSchemaNewValidCtxt(m_schema);
    if (!context)
    {
        if (error)
            error->assign(ErrorCode::InternalError, "Failed to initialise schema validator");
        return false;
    }

    if (error)
    {
        xmlSchemaSetValidErrors(context, &XmlSchemaValidator::onSchemaError, &XmlSchemaValidator::onSchemaError, error);
    }

    bool valid = (xmlSchemaValidateDoc(context, doc) == 0);
    if (valid && error)
    {
        error->clear();
    }

    xmlSchemaFreeValidCtxt(context);

    return valid;
}