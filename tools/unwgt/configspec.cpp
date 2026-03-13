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

#include <EntosTypes.h>
#include <PackageMetaData.h>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <string_view>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

namespace LIBRALF_NS
{
    // NOLINTBEGIN(misc-no-recursion): Allow recursive JSON types

    void to_json(nlohmann::json &json, const JSON &value);

    template <class>
    inline constexpr bool always_false_v = false;

    // -----------------------------------------------------------------------------
    /*!
        Helper function to convert a JSONValue to a nlohmann::json object.  This
        allows us to use the nlohmann::json library to serialize the JSON values.

    */
    void to_json(nlohmann::json &json, const JSONValue &value)
    {
        std::visit(
            [&](auto &&arg)
            {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    json = nlohmann::json::value_t::null;
                }
                else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int64_t> || std::is_same_v<T, double> ||
                                   std::is_same_v<T, std::string>)
                {
                    json = arg;
                }
                else if constexpr (std::is_same_v<T, std::vector<JSON>>)
                {
                    json = nlohmann::json::array();
                    for (const JSON &item : arg)
                        json.push_back(item.value);
                }
                else if constexpr (std::is_same_v<T, std::map<std::string, JSON>>)
                {
                    json = nlohmann::json::object();
                    for (const auto &[key, item] : arg)
                        json[key] = item.value;
                }
                else
                {
                    static_assert(always_false_v<T>, "non-exhaustive visitor!");
                }
            },
            value);
    }

    void to_json(nlohmann::json &json, const JSON &value)
    {
        to_json(json, value.value);
    }

    // NOLINTEND(misc-no-recursion)
} // namespace LIBRALF_NS

static nlohmann::json createPermissionsJson(const Permissions &permissions)
{
    nlohmann::json permissionsJson = nlohmann::json::array();

    for (const auto &privilege : permissions.all())
    {
        permissionsJson.push_back(privilege);
    }

    return permissionsJson;
}

static nlohmann::json createDependenciesJson(const PackageMetaData &metaData)
{
    nlohmann::json dependencies = nlohmann::json::object();

    for (const auto &[id, versionConstraint] : metaData.dependencies())
    {
        dependencies[id] = versionConstraint.toString();
    }

    return dependencies;
}

// -----------------------------------------------------------------------------
/*!
    Takes the parsed metadata from the package and converts it back into a JSON
    spec format compatible with the RDK OCI artifact spec.  This is the inverse
    of the `OCIPackageMetaDataImpl` code.

 */
std::string metaDataToConfigSpec(const PackageMetaData &metaData)
{
    nlohmann::ordered_json spec(nlohmann::json::value_t::object);

    spec["id"] = metaData.id();

    spec["version"] = metaData.version().toString();
    if (!metaData.versionName().empty())
        spec["versionName"] = metaData.versionName();

    if (metaData.title().has_value())
        spec["name"] = metaData.title().value();

    switch (metaData.type())
    {
        case PackageType::Application:
            spec["packageType"] = "application";
            spec["runtimeType"] = metaData.applicationInfo()->runtimeType();
            break;
        case PackageType::Service:
            spec["packageType"] = "service";
            spec["runtimeType"] = metaData.serviceInfo()->runtimeType();
            break;
        case PackageType::Runtime:
            spec["packageType"] = "runtime";
            break;
        case PackageType::Base:
            spec["packageType"] = "base";
            break;
        default:
            spec["packageType"] = "unknown";
            break;
    }

    spec["entryPointPath"] = metaData.entryPointPath();

#if 0
    const auto &icons = metaData.icons();
    if (!icons.empty())
    {
        nlohmann::ordered_json iconArray = nlohmann::json::array();
        for (const auto &icon : icons)
        {
            nlohmann::ordered_json iconJson = nlohmann::json::object();
            iconJson["src"] = icon.path.string();
            if (!icon.mimeType.empty())
                iconJson["type"] = icon.mimeType;
            if (!icon.purpose.empty())
                iconJson["purpose"] = icon.purpose;

            if (!icon.sizes.empty())
            {
                std::ostringstream ss;
                for (const auto &[width, height] : icon.sizes)
                    ss << width << "x" << height << " ";

                iconJson["sizes"] = ss.str();
            }

            iconArray.push_back(std::move(iconJson));
        }

        spec["icons"] = std::move(iconArray);
    }
#endif

    spec["dependencies"] = createDependenciesJson(metaData);

    if (metaData.type() == PackageType::Application)
        spec["permissions"] = createPermissionsJson(metaData.applicationInfo()->permissions());
    else if (metaData.type() == PackageType::Service)
        spec["permissions"] = createPermissionsJson(metaData.serviceInfo()->permissions());

    // We can cheat with the configuration, as the OCI packages store all the configuration in the vendorConfig map,
    // even if not actual vendor specific config.  So can just iterate over the vendorConfig map and add it to the
    // configuration object.
    nlohmann::json configJson(nlohmann::json::value_t::object);
    const auto configKeys = metaData.vendorConfigKeys();
    for (const auto &key : configKeys)
    {
        const auto &value = metaData.vendorConfig(key);
        if (!value.isNull())
            configJson[key] = value;
    }

    if (!configJson.empty())
        spec["configuration"] = configJson;

    return spec.dump(4);
}