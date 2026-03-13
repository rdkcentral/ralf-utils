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

#include "EntosTypes.h"
#include "FileUtils.h"
#include "core/Utils.h"
#include "oci/OCIPackageMetaDataImpl.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

namespace LIBRALF_NS
{
    static bool operator==(const Icon &lhs, const Icon &rhs)
    {
        return (lhs.path == rhs.path) && (lhs.mimeType == rhs.mimeType) && (lhs.purpose == rhs.purpose) &&
               (lhs.sizes == rhs.sizes);
    }

} // namespace LIBRALF_NS

TEST(OCIPackageConfigTests, checkBasicParsing)
{
    const auto contents = fileContents(TEST_DATA_DIR "/oci-config-01.json");
    ASSERT_GT(contents.size(), 64);

    const auto result = OCIPackageMetaDataImpl::fromConfigJson(contents);
    ASSERT_TRUE(result.hasValue()) << result.error().what();

    const auto &config = result.value();
    EXPECT_EQ(config->id(), "com.sky.myapp");
    EXPECT_EQ(config->version(), VersionNumber(1, 2, 3));
    EXPECT_EQ(config->versionName(), "1.2.3-beta");
    EXPECT_EQ(config->title(), "My Application");
    EXPECT_EQ(config->type(), PackageType::Application);
    EXPECT_EQ(config->mimeType(), "application/html");
    EXPECT_EQ(config->entryPointPath(), "web/index.html");
    EXPECT_EQ(config->entryArgs().size(), 0);

    const auto expectedConstraint = VersionConstraint::fromString(">=1.1.0");
    const std::map<std::string, VersionConstraint> expectedDeps = { { "rdk.browser.wpe", expectedConstraint.value() } };
    EXPECT_EQ(config->dependencies(), expectedDeps);

    const auto expectedIcons = std::list<Icon>{
        { "icon/low-res.png", "", "", { { 48, 48 } } },
        { "maskable_icon.png", "image/png", "", { { 128, 128 } } },
    };
    EXPECT_EQ(config->icons(), expectedIcons);

    ASSERT_NE(config->permissions(), nullptr);
    EXPECT_TRUE(config->permissions()->get("urn:rdk:permission:internet"));
    EXPECT_FALSE(config->permissions()->get("urn:rdk:permission:frustration"));

    EXPECT_EQ(config->vendorConfig("urn:entos:settings:sky-live-app").asBool(), true);
    EXPECT_EQ(config->vendorConfig("urn:entos:settings:parent-package-id").asString(), "com.sky.parentapp");
    EXPECT_EQ(config->vendorConfig("urn:rdk:config:log-levels").asArray(),
              std::vector<JSON>({ JSON("error"), JSON("warning"), JSON("info") }));

    const auto dial = config->vendorConfig("urn:rdk:settings:dial").asObject();
    EXPECT_EQ(dial.at("appNames").asArray(), std::vector<JSON>({ JSON("MyMediaApp"), JSON("MediaRemote") }));
    EXPECT_EQ(dial.at("corsDomains").asArray(),
              std::vector<JSON>({ JSON("http://example.com"), JSON("https://my-media.com") }));
    EXPECT_EQ(dial.at("originHeaderRequired").asBool(), true);
}

TEST(OCIPackageConfigTests, checkInvalidJson)
{
    auto contents = fileContents(TEST_DATA_DIR "/oci-config-02.json");
    ASSERT_GT(contents.size(), 64);

    const auto result = OCIPackageMetaDataImpl::fromConfigJson(contents);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.error().code(), ErrorCode::PackageContentsInvalid);
}

TEST(OCIPackageConfigTests, checkMemoryParsing)
{
    const auto contents = fileContents(TEST_DATA_DIR "/oci-config-01.json");
    ASSERT_GT(contents.size(), 64);

    const auto result = OCIPackageMetaDataImpl::fromConfigJson(contents);
    ASSERT_TRUE(result.hasValue()) << result.error().what();

    const auto &config = result.value();

    ASSERT_TRUE(config->memoryQuota().has_value());
    EXPECT_EQ(config->memoryQuota().value(), 256 * 1024 * 1024);

    ASSERT_TRUE(config->gpuMemoryQuota().has_value());
    EXPECT_EQ(config->gpuMemoryQuota().value(), 128 * 1024 * 1024);

    ASSERT_TRUE(config->storageQuota().has_value());
    EXPECT_EQ(config->storageQuota().value(), 200 * 1024 * 1024);
}

TEST(OCIPackageConfigTests, checkEntryArgsParsing)
{
    // Test invalid entry args
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "entryArgs": { "arg1": "value1" },
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.error().code(), ErrorCode::PackageContentsInvalid);
    }

    // Test invalid entry args
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "entryArgs": [ { "arg1": "value1" } ],
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.error().code(), ErrorCode::PackageContentsInvalid);
    }

    // Test empty entry args
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "entryArgs": [ ],
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_TRUE(result.hasValue()) << "Failed to parse json config" << result.error().what();
        EXPECT_EQ(result.value()->entryArgs().size(), 0);
    }

    // Test valid entry args
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "entryArgs": [ "arg1", "arg2", "--flag" ],
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_TRUE(result.hasValue()) << "Failed to parse json config" << result.error().what();

        const auto expectedArgs = std::list<std::string>{ "arg1", "arg2", "--flag" };
        EXPECT_EQ(result.value()->entryArgs(), expectedArgs);
    }
}

TEST(OCIPackageConfigTests, checkOverridesParsing)
{
    // Happy case
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            },
            "configuration": {
                "urn:rdk:config:overrides": {
                    "application": {
                        "setting1": "value1",
                        "setting2": 42
                    },
                    "runtime": [
                        {"settingA": true},
                        {"settingB": false}
                    ],
                    "base": "some string value"
                }
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_TRUE(result.hasValue()) << "Failed to parse json config - " << result.error().what();

        const auto &metaData = result.value();
        EXPECT_EQ(metaData->overrides(Override::Application), R"({"setting1":"value1","setting2":42})"_json);
        EXPECT_EQ(metaData->overrides(Override::Runtime), R"([{"settingA":true},{"settingB":false}])"_json);
        EXPECT_EQ(metaData->overrides(Override::Base), R"("some string value")"_json);
    }

    // Error case - overrides is not an object
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            },
            "configuration": {
                "urn:rdk:config:overrides": "this should be an object"
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.error().code(), ErrorCode::PackageContentsInvalid);
    }

    // Valid - but only application override present
    {
        const auto config = R"({
            "id": "com.sky.myapp",
            "version": "1.2.3",
            "packageType": "application",
            "entryPoint": "/usr/bin/frazzle",
            "dependencies": {
                "rdk.browser.wpe": ">=1.1.0"
            },
            "configuration": {
                "urn:rdk:config:overrides": {
                    "application": {
                        "setting1": "value1"
                    }
                }
            }
        })";

        const auto result = OCIPackageMetaDataImpl::fromConfigJson(
            std::vector<uint8_t>(reinterpret_cast<const char *>(config),
                                 reinterpret_cast<const char *>(config) + strlen(config)));
        ASSERT_TRUE(result.hasValue()) << "Failed to parse json config - " << result.error().what();

        const auto &metaData = result.value();
        EXPECT_EQ(metaData->overrides(Override::Application), R"({"setting1":"value1"})"_json);
        EXPECT_EQ(metaData->overrides(Override::Runtime), JSON());
        EXPECT_EQ(metaData->overrides(Override::Base), JSON());
    }
}
