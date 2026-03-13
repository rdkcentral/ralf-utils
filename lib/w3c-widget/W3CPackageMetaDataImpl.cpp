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

#include "W3CPackageMetaDataImpl.h"
#include "ConfigXmlSchemas.h"
#include "EntosTypes.h"
#include "core/LogMacros.h"
#include "core/Utils.h"
#include "core/XmlSchemaValidator.h"

#include <nlohmann/json.hpp>

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>

#include <arpa/inet.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace std::literals::string_view_literals;

/// FIXME: The default storage size for an app if the app just has the 'storage' capability.  This matches the
///  appsserviced code, but probably shouldn't be hard-coded here.
#define DEFAULT_STORAGE_SIZE (12ULL * 1024 * 1024)

/// The maximum storage size an app can request, this matches the hard limit in the appsserviced code.
#define STORAGE_LIMIT_MB (512ULL)

/// Some smart pointer helpers for libxml2 objects
using xmlCharUniquePtr = std::unique_ptr<xmlChar, std::function<void(xmlChar *_Nonnull)>>;
using xmlBufferUniquePtr = std::unique_ptr<xmlBuffer, std::function<void(xmlBuffer *_Nonnull)>>;

// -------------------------------------------------------------------------
/*!
    \static

    Constructs a W3CPackageMetaDataImpl object from the contents of a config.xml
    file.

 */
std::shared_ptr<W3CPackageMetaDataImpl> W3CPackageMetaDataImpl::fromConfigXml(const std::vector<uint8_t> &contents,
                                                                              Error *_Nullable error)
{
    xmlDocUniquePtr doc = xmlDocUniquePtr(xmlReadMemory(reinterpret_cast<const char *>(contents.data()),
                                                        static_cast<int>(contents.size()), nullptr, nullptr, 0),
                                          xmlFreeDoc);
    if (!doc)
    {
        if (error)
            error->assign(ErrorCode::XmlParseError, "Failed to parse the config.xml file");
        return nullptr;
    }

    // Check that the root element is a 'widget' and has a 'version' attribute that is either '1.0' or '2.0'
    xmlNodePtr root = xmlDocGetRootElement(doc.get());
    if (!root || !xmlStrEqual(root->name, BAD_CAST "widget"))
    {
        if (error)
            error->assign(ErrorCode::InvalidMetaData, "Invalid config.xml file, root element is not 'widget'");
        return nullptr;
    }

    auto version = xmlCharUniquePtr(xmlGetProp(root, BAD_CAST "version"), xmlFree);
    if (!version)
    {
        if (error)
            error->assign(ErrorCode::InvalidMetaData,
                          "Invalid config.xml file, missing 'version' attribute on root element");
        return nullptr;
    }

    auto impl = std::make_shared<W3CPackageMetaDataImpl>();

    int configXmlVersion = 0;
    if (xmlStrEqual(version.get(), BAD_CAST "1.0"))
    {
        // Schema validate the document
        XmlSchemaValidator schemaV1Check(CONFIG_XML_V1_SCHEMA, compileTimeStrLen(CONFIG_XML_V1_SCHEMA));
        if (!schemaV1Check.isValid() || !schemaV1Check.validate(doc.get(), error))
        {
            return nullptr;
        }

        configXmlVersion = 1;
    }
    else if (xmlStrEqual(version.get(), BAD_CAST "2.0"))
    {
        XmlSchemaValidator schemaV2Check(CONFIG_XML_V2_SCHEMA, compileTimeStrLen(CONFIG_XML_V2_SCHEMA));
        if (!schemaV2Check.isValid() || !schemaV2Check.validate(doc.get(), error))
        {
            return nullptr;
        }

        configXmlVersion = 2;
    }
    else
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "Unsupported widget version '%s' in config.xml",
                                   version.get());
        return nullptr;
    }

    // Process all the top level elements in the config.xml file
    for (xmlNodePtr child = root->children; child; child = child->next)
    {
        if (child->type != XML_ELEMENT_NODE)
            continue;

        if (xmlStrEqual(child->name, BAD_CAST "name"))
        {
            if (!processName(impl.get(), child, error))
                return nullptr;
        }
        else if (xmlStrEqual(child->name, BAD_CAST "icon"))
        {
            processIcon(impl.get(), child, error);
        }
        else if (xmlStrEqual(child->name, BAD_CAST "content"))
        {
            if (configXmlVersion == 1)
            {
                if (!processContentV1(impl.get(), child, error))
                    return nullptr;
            }
            else
            {
                if (!processContentV2(impl.get(), child, error))
                    return nullptr;
            }
        }
        else if (xmlStrEqual(child->name, BAD_CAST "capabilities"))
        {
            if (!processCapabilities(impl.get(), child, error))
                return nullptr;
        }
        else if (xmlStrEqual(child->name, BAD_CAST "parentalControl"))
        {
            if (!processParentalControl(impl.get(), child, error))
                return nullptr;
        }
        else if (xmlStrEqual(child->name, BAD_CAST "platformVersionFrom"))
        {
            // Ignore this element, it's never been used
        }
        else
        {
            logWarning("Unknown element '%s' in config.xml", child->name);
        }
    }

    // <name> and <content> are mandatory, so before we return the metadata object we need to check
    // we have the minimum required data.
    if (impl->m_id.empty() || impl->m_version.empty() || impl->m_type == PackageType::Unknown ||
        impl->m_entryPointPath.empty())
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "Missing mandatory elements in config.xml");
        return nullptr;
    }

    return impl;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Utility function strip leading and trailing whitespace from a string.
*/
std::string &W3CPackageMetaDataImpl::trimString(std::string &str)
{
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    return str;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Utility function to split a string on the delimiting character and return
    the set of strings.

    This is also trims out all spaces and non-printable characters prior
    to splitting.
 */
std::set<std::string> W3CPackageMetaDataImpl::splitStringToSet(const std::string &str, const char delim)
{
    std::istringstream ss(str);

    std::string value;
    std::set<std::string> values;
    while (std::getline(ss, value, delim))
    {
        trimString(value);
        values.emplace(std::move(value));
    }

    return values;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Same as splitStringToSet() but returns a vector of strings.
*/
std::vector<std::string> W3CPackageMetaDataImpl::splitStringToVector(const std::string &str, const char delim)
{
    std::istringstream ss(str);

    std::string value;
    std::vector<std::string> values;
    while (std::getline(ss, value, delim))
    {
        trimString(value);
        values.emplace_back(std::move(value));
    }

    return values;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Splits the string to a JSON array of strings, using the supplied delimiter
    character.  The strings are trimmed of leading and trailing whitespace
    and non-printable characters.

 */
JSON W3CPackageMetaDataImpl::splitStringToJsonArray(const std::string &str, const char delim)
{
    std::istringstream ss(str);

    std::string value;
    std::vector<JSON> values;
    while (std::getline(ss, value, delim))
    {
        trimString(value);
        values.emplace_back(std::move(value));
    }

    return JSON(std::move(values));
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the <name> element. This element is mandatory and contains the
    id and version of the package.

    On any error the \a error object is set and false is returned.
            */
bool W3CPackageMetaDataImpl::processName(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull nameElement,
                                         LIBRALF_NS::Error *_Nullable error)
{
    // The 'short' name of the package is the id of the package
    xmlAttrPtr attr = xmlHasProp(nameElement, BAD_CAST "short");
    if (!attr || !attr->children || !attr->children->content)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData,
                                   "<name> on line %ld is missing required 'short' attribute", XML_GET_LINE(nameElement));
        return false;
    }

    // There are certain rules on the format of a package id, so we need to check it has valid characters
    std::string shortName(reinterpret_cast<const char *>(attr->children->content));
    if (!verifyPackageId(shortName))
    {
        if (error)
            *error =
                Error::format(ErrorCode::InvalidMetaData, "Invalid package id '%s' in config.xml", shortName.c_str());
        return false;
    }

    metaData->m_id = std::move(shortName);

    // The 'version' attribute is mandatory as well, but not format restrictions on it, just that it cannot be empty
    attr = xmlHasProp(nameElement, BAD_CAST "version");
    if (!attr || !attr->children || !attr->children->content)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData,
                                   "<name> on line %ld is missing required 'version' attribute",
                                   XML_GET_LINE(nameElement));
        return false;
    }

    std::string version(reinterpret_cast<const char *>(attr->children->content));
    if (version.empty())
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "Version attribute in config.xml is empty");
        return false;
    }

    metaData->m_version = std::move(version);

    // The contents of the <name> element was suppose to be the human readable name of the package, we store that
    // as the title, but it's typically not used
    auto content = xmlCharUniquePtr(xmlNodeGetContent(nameElement), xmlFree);
    if (content)
    {
        metaData->m_title = std::string(reinterpret_cast<const char *>(content.get()));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the <icon> element. Icon is optional and contains the path to
    the icon file as well as mime type.

    In older versions of the config.xml file the <icon> element could have
    been specified multiple times with a 'usage' attribute.  However that is
    deprecated and it's just the last <icon> element that is used.

    On any error the \a error object is set and false is returned.
 */
bool W3CPackageMetaDataImpl::processIcon(W3CPackageMetaDataImpl *_Nonnull metaData, const xmlNode *_Nonnull iconElement,
                                         LIBRALF_NS::Error *_Nullable error)
{
    // Any failures parsing the <icon> element are not fatal, just warnings
    (void)error;

    // The 'short' name of the package is the id of the package
    xmlAttrPtr attr = xmlHasProp(iconElement, BAD_CAST "src");
    if (!attr || !attr->children || !attr->children->content)
    {
        logWarning("<icon> on line %ld is missing a 'src' attribute", XML_GET_LINE(iconElement));
        return true;
    }

    std::filesystem::path iconSrc = reinterpret_cast<const char *>(attr->children->content);
    if (iconSrc.empty())
    {
        logWarning("<icon> on line %ld has an empty 'src' attribute", XML_GET_LINE(iconElement));
        return true;
    }
    else if (!verifyPackagePath(iconSrc))
    {
        logWarning("<icon> on line %ld has an invalid 'src' path attribute", XML_GET_LINE(iconElement));
        return true;
    }

    Icon icon;
    icon.path = std::move(iconSrc);

    // The 'type' attribute is optional, but if present it should be a valid mime type
    attr = xmlHasProp(iconElement, BAD_CAST "type");
    if (attr && attr->children && attr->children->content)
    {
        icon.mimeType = std::string(reinterpret_cast<const char *>(attr->children->content));
    }

    metaData->m_icons.emplace_back(std::move(icon));

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the <content> element from version 1.0 format of the config.xml.
    This element is mandatory and contains entry point of the app or runtime
    and the type of the package, both attributes must be present.

    On 1.0 of files, it was possible to have multiple <content> elements,
    targeting different platforms.  This is no longer supported and only the
    last <content> tag is used.
 */
bool W3CPackageMetaDataImpl::processContentV1(W3CPackageMetaDataImpl *_Nonnull metaData,
                                              const xmlNode *_Nonnull contentElement, LIBRALF_NS::Error *_Nullable error)
{
    // The 'src' attribute is mandatory and is the entry point for the package
    xmlAttrPtr attr = xmlHasProp(contentElement, BAD_CAST "src");
    if (!attr || !attr->children || !attr->children->content)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "<content> on line %ld is missing 'src' attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }

    std::filesystem::path entryPoint(reinterpret_cast<const char *>(attr->children->content));
    if (entryPoint.empty())
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "<content> on line %ld has an empty 'src' attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }
    else if (!verifyPackagePath(entryPoint))
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData,
                                   "<content> on line %ld has an invalid 'src' path attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }

    metaData->m_entryPointPath = std::move(entryPoint);

    // The 'type' attribute is mandatory and is the type of the package
    attr = xmlHasProp(contentElement, BAD_CAST "type");
    if (!attr || !attr->children || !attr->children->content)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "<content> on line %ld is missing 'type' attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }

    std::string type(reinterpret_cast<const char *>(attr->children->content));
    if (type.empty())
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "<content> on line %ld has an empty 'type' attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }

    // The type should be prefixed with 'application/' or 'runtime/' as this determines the type
    if (type.rfind("application/", 0) == 0)
    {
        if (metaData->m_type == PackageType::Unknown)
            metaData->m_type = PackageType::Application;

        metaData->m_runtimeType = type.substr(12);
    }
    else if (type.rfind("runtime/", 0) == 0)
    {
        metaData->m_type = PackageType::Runtime;
    }
    else
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "<content> on line %ld has an invalid 'type' attribute",
                                   XML_GET_LINE(contentElement));
        return false;
    }

    metaData->m_contentType = std::move(type);

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the <content> element from version 2.0 format of the config.xml.
    The 'src' and 'type' attributes are mandatory and are the same format
    as for the 1.0 format.

    However in addition  the version 2.0 of the config.xml file also may
    contain a <platformFilter> child element which is optional and contains
    a set of platform filters that the package supports.
 */
bool W3CPackageMetaDataImpl::processContentV2(W3CPackageMetaDataImpl *_Nonnull metaData,
                                              const xmlNode *_Nonnull contentElement, LIBRALF_NS::Error *_Nullable error)
{
    // Process the <content> element as if it's a 1.0 type, ignoring the 'platform' attribute.  This will populate
    // the entry point and type of the metadata object.
    if (!processContentV1(metaData, contentElement, error))
        return false;

    std::vector<JSON> platformIds;
    std::vector<JSON> platformVariants;
    std::vector<JSON> countries;
    std::vector<JSON> subdivisions;
    std::vector<JSON> propositions;
    std::vector<JSON> yoctoVersions;

    // The <platformFilter> element is optional, we are also lax on the contents, there is only minimal sanity
    // checking performed, it's up to the client of the library to validate the platform filters make sense.
    for (xmlNodePtr child = contentElement->children; child; child = child->next)
    {
        if ((child->type == XML_ELEMENT_NODE) && xmlStrEqual(child->name, BAD_CAST "platformFilters"))
        {
            for (xmlNodePtr platformFilterNode = child->children; platformFilterNode;
                 platformFilterNode = platformFilterNode->next)
            {
                if (platformFilterNode->type != XML_ELEMENT_NODE)
                    continue;

                // Every platform filter should have a 'name' attribute
                xmlAttrPtr attr = xmlHasProp(platformFilterNode, BAD_CAST "name");
                if (!attr || !attr->children || !attr->children->content)
                {
                    logWarning("<%s> on line %ld is missing 'name' attribute, ignoring", platformFilterNode->name,
                               XML_GET_LINE(platformFilterNode));
                    continue;
                }

                std::string nameAttr(reinterpret_cast<const char *>(attr->children->content));
                if (nameAttr.empty())
                {
                    logWarning("<%s> on line %ld has empty missing 'name' attribute, ignoring",
                               platformFilterNode->name, XML_GET_LINE(platformFilterNode));
                    continue;
                }

                trimString(nameAttr);

                if (xmlStrEqual(platformFilterNode->name, BAD_CAST "platformId"))
                {
                    platformIds.emplace_back(std::move(nameAttr));
                }
                else if (xmlStrEqual(platformFilterNode->name, BAD_CAST "platformVariant"))
                {
                    platformVariants.emplace_back(std::move(nameAttr));
                }
                else if (xmlStrEqual(platformFilterNode->name, BAD_CAST "region") ||
                         xmlStrEqual(platformFilterNode->name, BAD_CAST "country"))
                {
                    countries.emplace_back(std::move(nameAttr));
                }
                else if (xmlStrEqual(platformFilterNode->name, BAD_CAST "subdivision"))
                {
                    subdivisions.emplace_back(std::move(nameAttr));
                }
                else if (xmlStrEqual(platformFilterNode->name, BAD_CAST "proposition"))
                {
                    propositions.emplace_back(std::move(nameAttr));
                }
                else if (xmlStrEqual(platformFilterNode->name, BAD_CAST "yoctoVersion"))
                {
                    yoctoVersions.emplace_back(std::move(nameAttr));
                }
                else
                {
                    logWarning("Unknown element '<%s>' in <platformFilters> element", platformFilterNode->name);
                }
            }

            break;
        }
    }

    std::map<std::string, JSON> platformFilters;
    if (!platformIds.empty())
        platformFilters.emplace("platformIds", JSON(std::move(platformIds)));
    if (!platformVariants.empty())
        platformFilters.emplace("platformVariants", JSON(std::move(platformVariants)));
    if (!countries.empty())
        platformFilters.emplace("countries", JSON(std::move(countries)));
    if (!subdivisions.empty())
        platformFilters.emplace("subdivisions", JSON(std::move(subdivisions)));
    if (!propositions.empty())
        platformFilters.emplace("propositions", JSON(std::move(propositions)));
    if (!yoctoVersions.empty())
        platformFilters.emplace("yoctoVersions", JSON(std::move(yoctoVersions)));

    if (!platformFilters.empty())
        metaData->m_vendorConfig.emplace(ENTOS_PLATFORM_FILTERS_CONFIGURATION, std::move(platformFilters));

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes all the <capability> elements within the <capabilities> element.

    On any error the \a error object is set and false is returned.
*/
bool W3CPackageMetaDataImpl::processCapabilities(W3CPackageMetaDataImpl *_Nonnull metaData,
                                                 const xmlNode *_Nonnull capabilitiesElement,
                                                 LIBRALF_NS::Error *_Nullable error)
{
    // Create a buffer to store the capability contents
    auto contentBuf = xmlBufferUniquePtr(xmlBufferCreate(), xmlBufferFree);

    for (xmlNodePtr capability = capabilitiesElement->children; capability; capability = capability->next)
    {
        if (capability->type != XML_ELEMENT_NODE)
            continue;

        if (!xmlStrEqual(capability->name, BAD_CAST "capability"))
        {
            logWarning("Unknown element '<%s>' in <capabilities> element", capability->name);
            continue;
        }

        xmlAttrPtr attr = xmlHasProp(capability, BAD_CAST "name");
        if (!attr || !attr->children || !attr->children->content)
        {
            if (error)
                *error = Error::format(ErrorCode::InvalidMetaData, "Capability on line %ld is missing 'name' attribute",
                                       XML_GET_LINE(capability));
            return false;
        }

        const std::string name(reinterpret_cast<const char *>(attr->children->content));

        // Get the content of the capability, this is optional
        if (xmlNodeBufGetContent(contentBuf.get(), capability) < 0)
        {
            if (error)
            {
                auto *xmlError = xmlGetLastError();
                *error = Error::format(ErrorCode::InvalidMetaData, "Capability on line %ld has invalid content - %s",
                                       XML_GET_LINE(capability), (xmlError ? xmlError->message : "Unknown error"));
            }

            return false;
        }

        std::string content;
        if (xmlBufferLength(contentBuf.get()) > 0)
        {
            content.assign(reinterpret_cast<const char *>(xmlBufferContent(contentBuf.get())),
                           xmlBufferLength(contentBuf.get()));
        }

        xmlBufferEmpty(contentBuf.get());

        // If we have some content then strip out leading and trailing whitespace
        if (!content.empty())
        {
            trimString(content);
        }

        // Process the capability
        if (!processCapability(metaData, name, content, error))
        {
            // Error already set
            return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes an individual <parentalControl> element, it's contents is
    expected to be "true" or "false".

    \note This is a deprecated element and is no longer used, however still
    supported for backwards compatibility.
 */
bool W3CPackageMetaDataImpl::processParentalControl(W3CPackageMetaDataImpl *_Nonnull metaData,
                                                    const xmlNode *_Nonnull parentalControlElement,
                                                    LIBRALF_NS::Error *_Nullable error)
{
    (void)error;

    xmlChar *content = xmlNodeGetContent(parentalControlElement);
    if (!content)
    {
        logWarning("<parentalControl> on line %ld is missing content", XML_GET_LINE(parentalControlElement));
        return true;
    }

    if (xmlStrcasecmp(content, BAD_CAST "true") == 0)
        metaData->m_vendorConfig.emplace(ENTOS_PARENTAL_CONTROL_CONFIGURATION, true);
    else if (xmlStrcasecmp(content, BAD_CAST "false") == 0)
        metaData->m_vendorConfig.emplace(ENTOS_PARENTAL_CONTROL_CONFIGURATION, false);
    else
        logWarning("Invalid content '%s' for <parentalControl> element, expected 'true' or 'false'", content);

    xmlFree(content);
    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes an individual <capability> element.

    On a fatal error the \a error object is set and false is returned.
 */
bool W3CPackageMetaDataImpl::processCapability(W3CPackageMetaDataImpl *_Nonnull metaData, const std::string &name,
                                               const std::string &content, LIBRALF_NS::Error *_Nullable error)
{
    logDebug("Processing capability '%s' with content '%s'", name.c_str(), content.c_str());

    // Check if the capability is one of the boolean privilege values
    static const std::multimap<std::string_view, std::string_view> kPermissionMap = {
        { "home-app"sv, HOME_APP_PERMISSION },
        { "home-app"sv, COMPOSITOR_PERMISSION },
        { "compositor-app"sv, COMPOSITOR_PERMISSION },
        { "wan-lan"sv, INTERNET_PERMISSION },
        { "lan-wan"sv, INTERNET_PERMISSION },
        { "issue-notifications"sv, OVERLAY_PERMISSION },
        { "local-services-1"sv, ENTOS_AS_ACCESS_LEVEL1_PERMISSION },
        { "local-services-2"sv, ENTOS_AS_ACCESS_LEVEL2_PERMISSION },
        { "local-services-3"sv, ENTOS_AS_ACCESS_LEVEL3_PERMISSION },
        { "local-services-4"sv, ENTOS_AS_ACCESS_LEVEL4_PERMISSION },
        { "local-services-5"sv, ENTOS_AS_ACCESS_LEVEL5_PERMISSION },
        { "post-intents"sv, ENTOS_AS_POST_INTENT_PERMISSION },
        { "firebolt"sv, FIREBOLT_PERMISSION },
        { "thunder"sv, THUNDER_PERMISSION },
        { "rialto"sv, RIALTO_PERMISSION },
        { "airplay2"sv, ENTOS_AIRPLAY_PERMISSION },
        { "chromecast"sv, ENTOS_CHROMECAST_PERMISSION },
        { "as-player"sv, ENTOS_AS_PLAYER_PERMISSION },
        { "game-controller"sv, GAME_CONTROLLER_PERMISSION },
        { "tsb-storage"sv, ENTOS_TIME_SHIFT_BUFFER_PERMISSION },
        { "usb-mass-storage"sv, READ_EXTERNAL_STORAGE_PERMISSION },
        { "read-external-storage"sv, READ_EXTERNAL_STORAGE_PERMISSION },
        { "write-external-storage"sv, WRITE_EXTERNAL_STORAGE_PERMISSION },
        { "bearer-token-authentication"sv, ENTOS_BEARER_TOKEN_AUTHENTICATION_PERMISSION },
        { "https-mutual-authentication"sv, ENTOS_HTTPS_MTLS_AUTHENTICATION_PERMISSION },
        { "stb-entitlements"sv, ENTOS_ENTITLEMENT_INFO_PERMISSION },
        { "memory-intensive"sv, ENTOS_MEMORY_INTENSIVE_PERMISSION },
    };

    auto range = kPermissionMap.equal_range(name);
    if (range.first != range.second)
    {
        for (auto it = range.first; it != range.second; ++it)
            metaData->m_permissions->set(it->second);

        return true;
    }

    // Otherwise check for specific capabilities that require additional processing
    using CapabilityHandler =
        std::function<bool(W3CPackageMetaDataImpl *_Nonnull, const std::string &, const std::string &, Error *_Nullable)>;
    static const std::map<std::string_view, CapabilityHandler> kHandlerMap = {
        { "system-app"sv, processCapabilitySystemApp },
        { "daemon-app"sv, processCapabilitySystemApp },
        { "child-app"sv, processCapabilityParentPackageId },
        { "suspend-mode"sv, processCapabilityLifecycleStates },
        { "hibernate-mode"sv, processCapabilityLifecycleStates },
        { "no-low-power-mode"sv, processCapabilityLifecycleStates },
        { "keymapping"sv, processCapabilityKeyMapping },
        { "forward-keymapping"sv, processCapabilityKeyMapping },
        { "storage"sv, processCapabilityStorageSize },
        { "private-storage"sv, processCapabilityStorageSize },
        { "dial-app"sv, processCapabilityDialInfo },
        { "dial-id"sv, processCapabilityDialInfo },
        { "dial-origin-mandatory"sv, processCapabilityDialInfo },
        { "game-mode"sv, processCapabilityDisplayInfo },
        { "virtual-resolution"sv, processCapabilityDisplayInfo },
        { "refresh-rate-60hz"sv, processCapabilityDisplayInfo },
        { "program-reference-level"sv, processCapabilityAudioInfo },
        { "sound-mode"sv, processCapabilityAudioInfo },
        { "sound-scene"sv, processCapabilityAudioInfo },
        { "fkps"sv, processCapabilityFKPS },
        { "hole-punch"sv, processCapabilityServices },
        { "local-socket-server"sv, processCapabilityServices },
        { "local-socket-client"sv, processCapabilityServices },
        { "mediarite-underlay"sv, processCapabilityMediarite },
        { "catalogue-id"sv, processCapabilityCatalogueId },
        { "content-partner-id"sv, processCapabilityContentPartnerId },
        { "age-rating"sv, processCapabilityAgeRating },
        { "age-policy"sv, processCapabilityAgePolicy },
        { "pre-launch"sv, processCapabilityPreLaunch },
        { "log-levels"sv, processCapabilityLogging },
        { "start-timeout-sec"sv, processCapabilityWatchdog },
        { "watchdog-timeout-sec"sv, processCapabilityWatchdog },
        { "sys-memory-limit"sv, processCapabilityMemoryLimits },
        { "gpu-memory-limit"sv, processCapabilityMemoryLimits },
        { "drm-type"sv, processCapabilityDrm },
        { "drm-store"sv, processCapabilityDrm },
        { "pin-management"sv, processCapabilityPinManagement },
        { "mapi"sv, processCapabilityMediarite },
        { "forward-multicast"sv, processCapabilityMulticast },
        { "multicast-server-socket"sv, processCapabilityMulticast },
        { "multicast-client-socket"sv, processCapabilityMulticast },
        { "intercept"sv, processCapabilityAppIntercept },
    };

    auto it = kHandlerMap.find(name);
    if (it != kHandlerMap.end())
    {
        return it->second(metaData, name, content, error);
    }

    // Finally the following is the list of deprecated capabilities that we ignore and don't both logging a warning about
    static const std::set<std::string_view> kDeprecatedCapabilities = {
        "voice-disabled"sv,
        "extended-kill-gracetime"sv,
    };

    if (kDeprecatedCapabilities.find(name) != kDeprecatedCapabilities.end())
    {
        logInfo("Capability '%s' is deprecated, ignoring", name.c_str());
        return true;
    }

    // Traditional we've ignored unknown capabilities, but we should probably log a warning
    logWarning("Unknown capability '%s' in config.xml, ignoring", name.c_str());

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'system-app' capability; this is just a boolean that
    indicates the app is a system app (aka a service)
*/
bool W3CPackageMetaDataImpl::processCapabilitySystemApp(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)content;
    (void)error;

    if (metaData->m_type == PackageType::Runtime)
    {
        logWarning("Ignoring '%s' capability on a non-app package", name.c_str());
    }
    else
    {
        metaData->m_type = PackageType::Service;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'keymapping' and 'forward-keymapping' capabilities to set
    the captured input keys for the app.

    \note The is no sanity checking on the key names, we just split the string
    on commas.
*/
bool W3CPackageMetaDataImpl::processCapabilityKeyMapping(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                         const std::string &content, Error *_Nullable error)
{
    (void)error;

    if (!metaData->m_inputHandlingInfo)
    {
        metaData->m_inputHandlingInfo = InputHandlingInfo();
    }

    if (name == "keymapping")
    {
        metaData->m_inputHandlingInfo->capturedKeys = splitStringToVector(content);
    }
    else if (name == "forward-keymapping")
    {
        metaData->m_inputHandlingInfo->monitoredKeys = splitStringToVector(content);
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'storage' and 'private-storage' capabilities to set the
    storage size for the app.
*/
bool W3CPackageMetaDataImpl::processCapabilityStorageSize(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                          const std::string &content, Error *_Nullable error)
{
    (void)error;

    if (name == "private-storage")
    {
        unsigned long diskStorageMb = strtoul(content.c_str(), nullptr, 0);
        if ((diskStorageMb == 0) || (diskStorageMb > STORAGE_LIMIT_MB))
        {
            logWarning("Disk storage capacity (%lu) not allowed", diskStorageMb);
        }
        else
        {
            metaData->m_storageQuota = static_cast<uint64_t>(diskStorageMb) * 1024 * 1024;
        }
    }

    if ((name == "storage") && !metaData->m_storageQuota)
    {
        metaData->m_storageQuota = DEFAULT_STORAGE_SIZE;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'dial-app', 'dial-id' and 'dial-origin-mandatory' capabilities
    to set the DIAL configuration for the app.

    The 'dial-app' capability may contain content which is comma delimited list
    of allowed CORS domains.

    The 'dial-id' capability should contain a comma delimited list of DIAL
    application ids that the DIAL server can match for the app.
 */
bool W3CPackageMetaDataImpl::processCapabilityDialInfo(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                       const std::string &content, Error *_Nullable error)
{
    (void)error;

    if (!metaData->m_dialInfo)
        metaData->m_dialInfo = DialInfo();

    if (name == "dial-app")
    {
        if (!content.empty())
        {
            metaData->m_dialInfo->corsDomains = splitStringToVector(content);
        }
    }
    else if (name == "dial-id")
    {
        metaData->m_dialInfo->dialIds = splitStringToVector(content);
        if (metaData->m_dialInfo->dialIds.empty())
        {
            logWarning("No DIAL ids in the 'dial-id' capability");
        }
    }
    else if (name == "dial-origin-mandatory")
    {
        metaData->m_dialInfo->originHeaderRequired = true;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'virtual-resolution', 'refresh-rate-60hz' and 'game-mode'
    capabilities to set the display information for the app.
 */
bool W3CPackageMetaDataImpl::processCapabilityDisplayInfo(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                          const std::string &content, Error *_Nullable error)
{
    (void)error;

    if (name == "virtual-resolution")
    {
        metaData->m_displayInfo.size = DisplaySize::Default;

        char *endptr = nullptr;
        unsigned long resolution = strtoul(content.c_str(), &endptr, 0);

        // Allow 'i' or 'p' to affixed on the end of the resolution, although it obviously has no affect
        if (*endptr != '\0' && *endptr != 'p' && *endptr != 'i')
        {
            logWarning("Invalid resolution string value, defaulting to 1080");
        }
        else
        {
            switch (resolution)
            {
                case 480:
                    metaData->m_displayInfo.size = DisplaySize::Size720x480;
                    break;
                case 576:
                    metaData->m_displayInfo.size = DisplaySize::Size720x576;
                    break;
                case 720:
                    metaData->m_displayInfo.size = DisplaySize::Size1280x720;
                    break;
                case 1080:
                    metaData->m_displayInfo.size = DisplaySize::Size1920x1080;
                    break;
                case 2160:
                    metaData->m_displayInfo.size = DisplaySize::Size3840x2160;
                    break;
                case 4320:
                    metaData->m_displayInfo.size = DisplaySize::Size7680x4320;
                    break;
                default:
                    logWarning("Invalid 'virtual-resolution' value - %lu (setting to 1080)", resolution);
                    break;
            }
        }
    }
    else if (name == "refresh-rate-60hz")
    {
        metaData->m_displayInfo.refreshRate = DisplayRefreshRate::SixtyHz;
    }
    else if (name == "game-mode")
    {
        if (strcasecmp(content.c_str(), "static") == 0)
            metaData->m_displayInfo.pictureMode = "gameModeStatic";
        else if (strcasecmp(content.c_str(), "dynamic") == 0)
            metaData->m_displayInfo.pictureMode = "gameModeDynamic";
        else
            logWarning("Invalid 'game-mode' value '%s', ignoring", content.c_str());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'sound-mode', 'sound-scene' and 'program-reference-level'
    capabilities to set the desired audio information for the app.
 */
bool W3CPackageMetaDataImpl::processCapabilityAudioInfo(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)error;

    if (name == "sound-mode")
    {
        metaData->m_audioInfo.soundMode = content;
    }
    else if (name == "sound-scene")
    {
        metaData->m_audioInfo.soundScene = content;
    }
    else if (name == "program-reference-level")
    {
        metaData->m_audioInfo.loudnessAdjustment = strtoul(content.c_str(), nullptr, 0);
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'fkps' capability, which should contain a comma delimited
    list of FKPS files that the app can access.
 */
bool W3CPackageMetaDataImpl::processCapabilityFKPS(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                   const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    std::vector<JSON> fkpsFiles;
    for (const auto &entry : splitStringToVector(content))
    {
        std::filesystem::path filePath(entry);
        if (!verifyPackagePath(filePath))
        {
            logWarning("Invalid 'fkps' file '%s', ignoring", entry.c_str());
        }
        else
        {
            fkpsFiles.emplace_back(filePath.string());
        }
    }

    if (!fkpsFiles.empty())
    {
        std::map<std::string, JSON> fkpsConfig;
        fkpsConfig["files"] = JSON(std::move(fkpsFiles));
        metaData->m_vendorConfig.emplace(ENTOS_FKPS_CONFIGURATION, JSON(std::move(fkpsConfig)));
    }

    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityLifecycleStates(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                              const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)content;

    if (name == "suspend-mode")
    {
        metaData->m_supportedLifecycleStates |= LifecycleStates::Suspended;
    }
    else if (name == "hibernate-mode")
    {
        metaData->m_supportedLifecycleStates |= LifecycleStates::Hibernated;
    }
    else if (name == "no-low-power-mode")
    {
        metaData->m_supportedLifecycleStates &= ~LifecycleStates::LowPower;
    }

    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityParentPackageId(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                              const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    if (content.empty() || !verifyPackageId(content))
        logWarning("Invalid 'child-app' value '%s', ignoring", content.c_str());
    else
        metaData->m_storageGroup = content;

    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityCatalogueId(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                          const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    metaData->m_vendorConfig.emplace(ENTOS_CATALOGUE_ID_CONFIGURATION, JSON(content));
    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityContentPartnerId(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                               const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    metaData->m_vendorConfig.emplace(ENTOS_CONTENT_PARTNER_ID_CONFIGURATION, JSON(content));
    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityAgeRating(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    const auto ageRating = strtoul(content.c_str(), nullptr, 0);
    if (ageRating != ULLONG_MAX)
        metaData->m_vendorConfig.emplace(ENTOS_AGE_RATING_CONFIGURATION, static_cast<uint64_t>(ageRating));

    return true;
}

bool W3CPackageMetaDataImpl::processCapabilityAgePolicy(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;
    (void)content;

    metaData->m_vendorConfig.emplace(ENTOS_AGE_POLICY_CONFIGURATION, JSON(content));
    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'start-timeout-sec' and 'watchdog-sec' capabilities which
    should both contain the number of seconds for the timeout.
 */
bool W3CPackageMetaDataImpl::processCapabilityWatchdog(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                       const std::string &content, Error *_Nullable error)
{
    try
    {
        unsigned long secs = std::stoul(content);

        if (name == "start-timeout-sec")
            metaData->m_startTimeout = std::chrono::seconds(std::max(5UL, secs));
        else if (name == "watchdog-timeout-sec")
            metaData->m_watchdogInterval = std::chrono::seconds(std::max(5UL, secs));
    }
    catch (const std::exception &e)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "Invalid '%s' value '%s'", name.c_str(), content.c_str());

        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'log-levels' capability which should contain a comma
    delimited list of log levels that the app can use.
 */
bool W3CPackageMetaDataImpl::processCapabilityLogging(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                      const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    const auto levels = splitStringToSet(content);
    for (const auto &level : levels)
    {
        if (strcasecmp(level.c_str(), "default") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Default;
        else if (strcasecmp(level.c_str(), "debug") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Debug;
        else if (strcasecmp(level.c_str(), "info") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Info;
        else if (strcasecmp(level.c_str(), "milestone") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Milestone;
        else if (strcasecmp(level.c_str(), "warning") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Warning;
        else if (strcasecmp(level.c_str(), "error") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Error;
        else if (strcasecmp(level.c_str(), "fatal") == 0)
            metaData->m_loggingLevels |= LoggingLevel::Fatal;
        else
            logWarning("Invalid log level '%s' in 'log-levels' capability", level.c_str());
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'pre-launch' capability which should contain the name of
    mode which should match the enums in the PreloadConfig enum.
 */
bool W3CPackageMetaDataImpl::processCapabilityPreLaunch(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)error;
    (void)name;

    if (strcasecmp(content.c_str(), "allowed") == 0)
        metaData->m_vendorConfig.emplace(ENTOS_PRELAUNCH_CONFIGURATION, JSON("allowed"));
    else if (strcasecmp(content.c_str(), "recent") == 0)
        metaData->m_vendorConfig.emplace(ENTOS_PRELAUNCH_CONFIGURATION, JSON("recent"));
    else if (strcasecmp(content.c_str(), "never") == 0)
        metaData->m_vendorConfig.emplace(ENTOS_PRELAUNCH_CONFIGURATION, JSON("never"));

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'hole-punch', 'local-socket-server' and 'local-socket-client'
    capabilities which should each contain a comma delimited list of network
    ports to expose or map into a container.
*/
bool W3CPackageMetaDataImpl::processCapabilityServices(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                       const std::string &content, Error *_Nullable error)
{
    (void)error;

    const auto ports = splitStringToVector(content);
    for (const auto &protocolAndPort : ports)
    {
        NetworkService service;

        const auto pos = protocolAndPort.find(':');
        if (pos == std::string::npos)
        {
            service.protocol = NetworkService::TCP;
            service.port = static_cast<uint16_t>(strtoul(protocolAndPort.c_str(), nullptr, 0));
        }
        else
        {
            const auto protocol = protocolAndPort.substr(0, pos);
            if (strcasecmp(protocol.c_str(), "tcp") == 0)
                service.protocol = NetworkService::TCP;
            else if (strcasecmp(protocol.c_str(), "udp") == 0)
                service.protocol = NetworkService::UDP;
            else
            {
                logWarning("Invalid protocol '%s' in '%s' capability", protocol.c_str(), name.c_str());
                continue;
            }

            const auto port = protocolAndPort.substr(pos + 1);
            service.port = static_cast<uint16_t>(strtoul(port.c_str(), nullptr, 0));
        }

        if ((service.port <= 1024) || (service.port >= 65535))
        {
            logWarning("Invalid port '%s' in '%s' capability", protocolAndPort.c_str(), name.c_str());
            continue;
        }

        if (name == "hole-punch")
            metaData->m_publicServices.emplace_back(std::move(service));
        else if (name == "local-socket-server")
            metaData->m_exportedServices.emplace_back(std::move(service));
        else if (name == "local-socket-client")
            metaData->m_importedServices.emplace_back(std::move(service));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'sys-memory-limit' and 'gpu-memory-limit' capabilities which
    sets the memory limits.
 */
bool W3CPackageMetaDataImpl::processCapabilityMemoryLimits(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                           const std::string &content, Error *_Nullable error)
{
    try
    {
        size_t endPos = 0;
        uint64_t limit = std::stoull(content, &endPos, 0);
        if (endPos < content.length())
        {
            char lastChar = content.at(endPos);
            if (lastChar == 'k' || lastChar == 'K')
                limit *= 1024;
            else if (lastChar == 'm' || lastChar == 'M')
                limit *= (1024 * 1024);
            else if (lastChar == 'g' || lastChar == 'G')
                limit *= (1024 * 1024 * 1024);
        }

        if (name == "sys-memory-limit")
            metaData->m_memoryQuota = limit;
        else if (name == "gpu-memory-limit")
            metaData->m_gpuMemoryQuota = limit;
    }
    catch (const std::exception &)
    {
        if (error)
            *error = Error::format(ErrorCode::InvalidMetaData, "Invalid '%s' value '%s'", name.c_str(), content.c_str());

        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'drm-type' and 'drm-store' capabilities. The 'drm-store' is
    a legacy capability that is not used.  But the `drm-type` capability is
    exposed in the meta-data.
 */
bool W3CPackageMetaDataImpl::processCapabilityDrm(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                  const std::string &content, Error *_Nullable error)
{
    (void)error;

    std::map<std::string, JSON> emptyObj = {};
    auto it = metaData->m_vendorConfig.try_emplace(ENTOS_LEGACY_DRM_CONFIGURATION, JSON(emptyObj)).first;

    auto &config = it->second.asObject();

    if (name == "drm-type")
    {
        config["types"] = splitStringToJsonArray(content);
    }
    else if (name == "drm-store")
    {
        unsigned long storageSize = strtoul(content.c_str(), nullptr, 0);
        if (storageSize != ULONG_MAX)
            config["storageSize"] = JSON(static_cast<uint64_t>(storageSize * 1024));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'pin-management' capability, this is sky specific and is
    considered a deprecated capability.
 */
bool W3CPackageMetaDataImpl::processCapabilityPinManagement(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                            const std::string &content, Error *_Nullable error)
{
    (void)name;
    (void)error;

    // The pin management value has changes a few times, from "READ-WRITE" to "readwrite" and few others in between,
    // to maintain sanity we remove any dashes and then perform caseless string compare
    std::string value = content;
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());

    if (strcasecmp(value.c_str(), "readwrite") == 0)
        value = "readwrite";
    else if (strcasecmp(value.c_str(), "readonly") == 0)
        value = "readonly";
    else
        value = "excluded";

    metaData->m_vendorConfig.emplace(ENTOS_PIN_MANAGEMENT_CONFIGURATION, JSON(std::move(value)));
    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'mapi' capability, the body of the capability is a semi-colon
    delimited list of MAPI names with capabilities that the app wants.

 */
bool W3CPackageMetaDataImpl::processCapabilityMediarite(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)
{
    (void)error;

    std::map<std::string, JSON> emptyObj = {};
    auto it = metaData->m_vendorConfig.try_emplace(ENTOS_MEDIARITE_CONFIGURATION, JSON(emptyObj)).first;

    auto &config = it->second.asObject();

    if (name == "mediarite-underlay")
    {
        config.emplace("underlay", JSON(true));
    }
    else if (name == "mapi")
    {
        std::map<std::string, JSON> mapiAccessGroups;

        std::istringstream mapiStream(content);
        std::string group;
        while (std::getline(mapiStream, group, ';'))
        {
            auto pos = group.find(':');
            if (pos != std::string::npos)
            {
                std::string mapiName = group.substr(0, pos);
                mapiAccessGroups[mapiName] = splitStringToJsonArray(group.substr(pos + 1));
            }
            else
            {
                logWarning("Invalid value for 'mapi' field '%s'", group.c_str());
            }
        }

        if (!mapiAccessGroups.empty())
            config.emplace("accessGroups", std::move(mapiAccessGroups));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
    \static
    \internal

    Processes the 'forward-multicast', 'multicast-server-socket' and
    'multicast-client-socket' capabilities.

 */
bool W3CPackageMetaDataImpl::processCapabilityMulticast(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                        const std::string &content, Error *_Nullable error)

{
    (void)error;

    std::map<std::string, JSON> emptyObj = {};
    auto it = metaData->m_vendorConfig.try_emplace(ENTOS_MULTICAST_CONFIGURATION, JSON(emptyObj)).first;

    auto &config = it->second.asObject();

    if (name == "forward-multicast")
    {
        std::vector<JSON> multicastForwards;
        const std::set<std::string> sockets = splitStringToSet(content, ',');
        for (const auto &socket : sockets)
        {
            // The content of the 'forward-multicast' capability is <IP>:<PORT>
            auto parts = splitStringToVector(socket, ':');
            if (parts.size() != 2)
            {
                logWarning("Invalid 'forward-multicast' value '%s', expected <IP>:<PORT>", socket.c_str());
                continue;
            }

            in_addr ipAddr = { 0 };
            if (!inet_pton(AF_INET, parts[0].c_str(), &ipAddr))
            {
                logWarning("Invalid IP address '%s' in 'forward-multicast' capability", parts[0].c_str());
                continue;
            }

            unsigned port = strtoul(parts[1].c_str(), nullptr, 0);
            if ((port <= 1024u) || (port >= 65535u))
            {
                logWarning("Invalid port '%s' in 'forward-multicast' capability", parts[1].c_str());
                continue;
            }

            multicastForwards.emplace_back(JSON({ { "address", JSON(parts[0]) }, { "port", JSON(port) } }));
        }

        if (!multicastForwards.empty())
            config.emplace("forwarding", std::move(multicastForwards));
    }
    else if (name == "multicast-server-socket")
    {
        std::vector<JSON> multicastSockets;
        const std::set<std::string> sockets = splitStringToSet(content, ',');
        for (const auto &socket : sockets)
        {
            // The content of the 'multicast-server-socket' capability is <NAME>:<IP>:<PORT>
            auto parts = splitStringToVector(socket, ':');
            if (parts.size() != 3)
            {
                logWarning("Invalid 'multicast-server-socket' value '%s', expected <NAME>:<IP>:<PORT>", socket.c_str());
                continue;
            }

            in_addr ipAddr = { 0 };
            if (!inet_pton(AF_INET, parts[1].c_str(), &ipAddr))
            {
                logSysWarning(errno, "Invalid IP address '%s' in 'multicast-server-socket' capability", parts[1].c_str());
                continue;
            }

            char ipAddrStr[INET_ADDRSTRLEN + 1] = { 0 };
            if (inet_ntop(AF_INET, &ipAddr, ipAddrStr, INET_ADDRSTRLEN) == nullptr)
            {
                logSysWarning(errno, "Invalid IP address '%s' in 'multicast-server-socket' capability", parts[1].c_str());
                continue;
            }

            unsigned port = strtoul(parts[2].c_str(), nullptr, 0);
            if ((port <= 1024u) || (port >= 65535u))
            {
                logWarning("Invalid port '%s' in 'multicast-server-socket' capability", parts[2].c_str());
                continue;
            }

            multicastSockets.emplace_back(
                JSON({ { "name", JSON(parts[0]) }, { "address", JSON(ipAddrStr) }, { "port", JSON(port) } }));
        }

        if (!multicastSockets.empty())
            config.emplace("serverSockets", std::move(multicastSockets));
    }
    else if (name == "multicast-client-socket")
    {
        const std::set<std::string> socketNames = splitStringToSet(content, ',');

        std::vector<JSON> multicastSockets;
        multicastSockets.reserve(socketNames.size());

        for (const auto &socketName : socketNames)
        {
            multicastSockets.emplace_back(JSON({ { "name", JSON(socketName) } }));
        }

        if (!multicastSockets.empty())
            config.emplace("clientSockets", std::move(multicastSockets));
    }

    return true;
}

// -------------------------------------------------------------------------
/*!
        \static
        \internal
        Processes the 'intercept' capability, which is a boolean that indicates
        if the app wants to show an intercept on launch.
 */

bool W3CPackageMetaDataImpl::processCapabilityAppIntercept(W3CPackageMetaDataImpl *metaData, const std::string &name,
                                                           const std::string &content, Error *_Nullable error)
{
    (void)name;
    (void)error;

    bool interceptEnabled = false;

    if (strcasecmp(content.c_str(), "true") == 0)
    {
        interceptEnabled = true;
    }
    else if (strcasecmp(content.c_str(), "false") == 0)
    {
        interceptEnabled = false;
    }
    else
    {
        logWarning("Invalid 'intercept' value '%s', ignoring", content.c_str());
        return true;
    }

    std::map<std::string, JSON> interceptConfig;
    interceptConfig["enable"] = JSON(interceptEnabled);

    metaData->m_vendorConfig.emplace(ENTOS_MARKETPLACE_INTERCEPT_CONFIGURATION, JSON(interceptConfig));
    return true;
}

W3CPackageMetaDataImpl::W3CPackageMetaDataImpl()
    : m_permissions(std::make_shared<PermissionsImpl>())
{
}

const std::string &W3CPackageMetaDataImpl::id() const
{
    return m_id;
}

VersionNumber W3CPackageMetaDataImpl::version() const
{
    // Within config.xml we stopped requiring semver for the version number, this was a mistake IMO, but means
    // we can't use the VersionNumber object to parse the version number.  Instead, we return the "version" in the
    // versionName() field - clients will have to deal with this.

    return {};
}

const std::string &W3CPackageMetaDataImpl::versionName() const
{
    return m_version;
}

PackageType W3CPackageMetaDataImpl::type() const
{
    return m_type;
}

const std::string &W3CPackageMetaDataImpl::mimeType() const
{
    return m_contentType;
}

const std::string &W3CPackageMetaDataImpl::runtimeType() const
{
    return m_runtimeType;
}

const std::optional<std::string> &W3CPackageMetaDataImpl::title() const
{
    return m_title;
}

const PlatformInfo &W3CPackageMetaDataImpl::platformInfo() const
{
    return m_platformInfo;
}

const std::filesystem::path &W3CPackageMetaDataImpl::entryPointPath() const
{
    return m_entryPointPath;
}

const std::list<std::string> &W3CPackageMetaDataImpl::entryArgs() const
{
    // W3C widgets don't support entry args

    static const std::list<std::string> kEmptyEntryArgs;
    return kEmptyEntryArgs;
}

const std::map<std::string, LIBRALF_NS::VersionConstraint> &W3CPackageMetaDataImpl::dependencies() const
{
    // W3C widgets don't describe their dependencies in terms of package id and version, instead it solely relies on the
    // in the mimeType of the package.  So we return an empty map here.

    static const std::map<std::string, LIBRALF_NS::VersionConstraint> kEmptyDependencies;
    return kEmptyDependencies;
}

const std::list<LIBRALF_NS::Icon> &W3CPackageMetaDataImpl::icons() const
{
    return m_icons;
}

std::shared_ptr<IPermissionsImpl> W3CPackageMetaDataImpl::permissions() const
{
    return m_permissions;
}

std::optional<uint64_t> W3CPackageMetaDataImpl::storageQuota() const
{
    return m_storageQuota;
}

const std::optional<std::string> &W3CPackageMetaDataImpl::sharedStorageGroup() const
{
    return m_storageGroup;
}

std::optional<uint64_t> W3CPackageMetaDataImpl::memoryQuota() const
{
    return m_memoryQuota;
}

std::optional<uint64_t> W3CPackageMetaDataImpl::gpuMemoryQuota() const
{
    return m_gpuMemoryQuota;
}

LifecycleStates W3CPackageMetaDataImpl::supportedLifecycleStates() const
{
    return m_supportedLifecycleStates;
}

const std::vector<NetworkService> &W3CPackageMetaDataImpl::publicServices() const
{
    return m_publicServices;
}

const std::vector<NetworkService> &W3CPackageMetaDataImpl::exportedServices() const
{
    return m_exportedServices;
}

const std::vector<NetworkService> &W3CPackageMetaDataImpl::importedServices() const
{
    return m_importedServices;
}

const std::optional<DialInfo> &W3CPackageMetaDataImpl::dialInfo() const
{
    return m_dialInfo;
}

const std::optional<InputHandlingInfo> &W3CPackageMetaDataImpl::inputHandlingInfo() const
{
    return m_inputHandlingInfo;
}

const DisplayInfo &W3CPackageMetaDataImpl::displayInfo() const
{
    return m_displayInfo;
}

const AudioInfo &W3CPackageMetaDataImpl::audioInfo() const
{
    return m_audioInfo;
}

const std::optional<std::chrono::milliseconds> &W3CPackageMetaDataImpl::startTimeout() const
{
    return m_startTimeout;
}

const std::optional<std::chrono::milliseconds> &W3CPackageMetaDataImpl::watchdogInterval() const
{
    return m_watchdogInterval;
}

LoggingLevels W3CPackageMetaDataImpl::loggingLevels() const
{
    return m_loggingLevels;
}

JSON W3CPackageMetaDataImpl::vendorConfig(std::string_view key) const
{
    auto it = m_vendorConfig.find(key);
    if (it != m_vendorConfig.end())
        return it->second;
    else
        return {};
}

std::set<std::string> W3CPackageMetaDataImpl::vendorConfigKeys() const
{
    std::set<std::string> keys;

    for (const auto &pair : m_vendorConfig)
        keys.emplace(pair.first);

    return keys;
}

LIBRALF_NS::JSON W3CPackageMetaDataImpl::overrides(LIBRALF_NS::Override type) const
{
    (void)type;

    // W3C widgets don't support overrides

    return {};
}

bool W3CPackageMetaDataImpl::hasAuxMetaDataFile(std::string_view mediaType) const
{
    return m_auxMetaDataFiles.find(mediaType) != m_auxMetaDataFiles.end();
}

bool W3CPackageMetaDataImpl::addAuxMetaDataFile(std::string_view mediaType, std::vector<uint8_t> data)
{
    return m_auxMetaDataFiles.emplace(mediaType, std::move(data)).second;
}

Result<std::unique_ptr<PackageAuxMetaDataImpl>> W3CPackageMetaDataImpl::getAuxMetaDataFile(std::string_view mediaType) const
{
    auto it = m_auxMetaDataFiles.find(mediaType);
    if (it == m_auxMetaDataFiles.end())
    {
        return Error::format(ErrorCode::FileNotFound, "Auxiliary meta-data file for media type '%.*s' not found",
                             static_cast<int>(mediaType.size()), mediaType.data());
    }

    return std::make_unique<PackageAuxMetaDataImpl>(mediaType, 0, it->second);
}

std::set<std::string> W3CPackageMetaDataImpl::availAuxMetaData() const
{
    std::set<std::string> auxMetaData;
    for (const auto &pair : m_auxMetaDataFiles)
    {
        auxMetaData.insert(pair.first);
    }

    return auxMetaData;
}
