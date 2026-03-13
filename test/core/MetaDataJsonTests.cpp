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

#include "PackageMetaData.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

namespace LIBRALF_NS
{
    extern void from_json(const nlohmann::json &json, JSON &value);
    extern void from_json(const nlohmann::json &json, JSONValue &value);
} // namespace LIBRALF_NS

TEST(MetaDataJsonTests, fromJson)
{
    nlohmann::json json = R"({
        "id": "com.example.app",
        "foo": true,
        "bar": false,
        "largeInt": 4611686018427387904,
        "largeIntNeg": -4611686018427387904,
        "double": 3.14159265358979323846,
        "noValue": null,
        "version": "1.0.0",
        "entryPoint": "app/main.js",
        "dependencies": {
            "com.example.lib": ">=1.0.0"
        },
        "icons": [
            {
                "path": "/icons/icon.png",
                "mimeType": "image/png",
                "purpose": ["primary"],
                "sizes": [64, 128]
            },
            {
                "path": "/icons/icon2.png"
            }
        ],
        "permissions": {
            "networkAccess": true,
            "fileAccess": false
        },
        "settings": {
            "theme": {
                "color": "#FFFFFF"
            }
        },
        "arrayOfArrays": [
            [1, 2, 3],
            [4, 5, 6]
        ]
    })"_json;

    JSON jsonValue;
    from_json(json, jsonValue);

    EXPECT_TRUE(jsonValue.isObject());
    EXPECT_EQ(jsonValue.asObject().size(), 14);

    const auto root = jsonValue.asObject();

    ASSERT_TRUE(root.at("id").isString());
    EXPECT_EQ(root.at("id").asString(), "com.example.app");

    ASSERT_TRUE(root.at("foo").isBool());
    EXPECT_EQ(root.at("foo").asBool(), true);
    ASSERT_TRUE(root.at("bar").isBool());
    EXPECT_EQ(root.at("bar").asBool(), false);

    ASSERT_TRUE(root.at("largeInt").isInteger());
    EXPECT_EQ(root.at("largeInt").asInteger(), 4611686018427387904);
    ASSERT_TRUE(root.at("largeIntNeg").isInteger());
    EXPECT_EQ(root.at("largeIntNeg").asInteger(), -4611686018427387904);

    ASSERT_TRUE(root.at("double").isDouble());
    EXPECT_EQ(root.at("double").asDouble(), 3.14159265358979323846);

    ASSERT_TRUE(root.at("noValue").isNull());

    ASSERT_TRUE(root.at("dependencies").isObject());
    EXPECT_EQ(root.at("dependencies").asObject().at("com.example.lib"), JSON(">=1.0.0"));

    ASSERT_TRUE(root.at("icons").isArray());
    EXPECT_EQ(root.at("icons").asArray(),
              (std::vector<JSON>{
                  JSON(std::map<std::string, JSON>{ { "path", JSON("/icons/icon.png") },
                                                    { "mimeType", JSON("image/png") },
                                                    { "purpose", JSON(std::vector<JSON>{ JSON("primary") }) },
                                                    { "sizes", JSON(std::vector<JSON>{ JSON(64), JSON(128) }) } }),
                  JSON(std::map<std::string, JSON>{ { "path", JSON("/icons/icon2.png") } }),
              }));

    ASSERT_TRUE(root.at("settings").isObject());
    ASSERT_TRUE(root.at("settings").asObject().at("theme").isObject());
    EXPECT_EQ(root.at("settings").asObject().at("theme"),
              JSON(std::map<std::string, JSON>{ { "color", JSON("#FFFFFF") } }));

    ASSERT_TRUE(root.at("arrayOfArrays").isArray());
    ASSERT_EQ(root.at("arrayOfArrays").asArray().size(), 2);
    ASSERT_TRUE(root.at("arrayOfArrays").asArray()[0].isArray());
    ASSERT_TRUE(root.at("arrayOfArrays").asArray()[1].isArray());
    EXPECT_EQ(root.at("arrayOfArrays").asArray(), (std::vector<JSON>{
                                                      JSON(std::vector<JSON>{ JSON(1), JSON(2), JSON(3) }),
                                                      JSON(std::vector<JSON>{ JSON(4), JSON(5), JSON(6) }),
                                                  }));
}
