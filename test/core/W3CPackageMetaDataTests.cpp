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
#include "PackageMetaData.h"
#include "core/Utils.h"
#include "w3c-widget/W3CPackageMetaDataImpl.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace std::chrono_literals;

namespace LIBRALF_NS
{
    bool operator==(const NetworkService &lhs, const NetworkService &rhs)
    {
        return (lhs.name == rhs.name) && (lhs.protocol == rhs.protocol) && (lhs.port == rhs.port);
    }

} // namespace LIBRALF_NS

class TestPackageMetaData : public PackageMetaData
{
public:
    explicit TestPackageMetaData(std::shared_ptr<IPackageMetaDataImpl> impl)
        : PackageMetaData(std::move(impl))
    {
    }
};

static std::vector<uint8_t> generateConfigXmlWithCapability(const std::string &name, const std::string &value = "")
{
    const std::string capability = value.empty() ? R"(<capability name=")" + name + R"("/>)"
                                                 : R"(<capability name=")" + name + R"(">)" + value + R"(</capability>)";

    const std::string configXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<widget xmlns="http://www.bskyb.com/ns/widgets" version="2.0">
    <name short="fruit.tart" version="0.0.1">Fruit Tarts!!</name>
    <icon src="icon.png" type="image/png"/>
    <content src="start_client.sh" type="application/system">
        <platformFilters>
        </platformFilters>
    </content>
    <capabilities>
        )" + capability + R"(
    </capabilities>
</widget>
)";

    return { configXml.begin(), configXml.end() };
}

static JSON strToJson(const char *str)
{
    nlohmann::json json = nlohmann::json::parse(str);
    return json.get<JSON>();
}

TEST(PackageMetaDataTest, testSchemaCheck)
{
    Error error;

    const auto configXml = generateConfigXmlWithCapability("home-app");
    auto metaData = W3CPackageMetaDataImpl::fromConfigXml(configXml, &error);
    EXPECT_FALSE(error) << error.what();
    ASSERT_NE(metaData, nullptr);
}

TEST(PackageMetaDataTest, testPlatformFilters)
{
    Error error;

    const char *configXml = R"(
    <widget xmlns="http://www.bskyb.com/ns/widgets" version="2.0">
        <name short="com.sky.dvbapp" version="10.0.39.99">TV Input</name>
        <icon src="icon.png" type="image/png"/>
        <content src="." type="application/html">
            <platformFilters>
                <platformId name="donkey"/>
                <platformId name=""/>
                <platformId name=" billy "/>
                <platformVariant name="RDK-STB"/>
                <platformVariant name="RDK-PANEL"/>
                <region name="DEU"/>
                <region name="AUT"/>
                <proposition name="SKYQ"/>
                <country name="NZL"/>
                <subdivision name="GBR-SCT"/>
                <yoctoVersion name="KIRKSTONE"/>
            </platformFilters>
        </content>
        <capabilities>
            <capability name="wan-lan"/>
        </capabilities>
        <parentalControl>true</parentalControl>
    </widget>
    )";

    auto metaData = W3CPackageMetaDataImpl::fromConfigXml({ configXml, configXml + strlen(configXml) }, &error);
    EXPECT_FALSE(error) << error.what();
    ASSERT_NE(metaData, nullptr);

    TestPackageMetaData test(metaData);
    ASSERT_TRUE(test.isValid());
    ASSERT_FALSE(test.isNull());
    EXPECT_EQ(test.id(), "com.sky.dvbapp");
    EXPECT_EQ(test.versionName(), "10.0.39.99");
    EXPECT_EQ(test.title(), "TV Input");
    EXPECT_EQ(test.type(), PackageType::Application);
    ASSERT_TRUE(test.applicationInfo().has_value());
    EXPECT_TRUE(test.applicationInfo()->permissions().get(INTERNET_PERMISSION));

    const auto platformFilters = test.vendorConfig(ENTOS_PLATFORM_FILTERS_CONFIGURATION);
    ASSERT_TRUE(platformFilters.isObject());

    const auto &platformFiltersObj = platformFilters.asObject();
    EXPECT_EQ(platformFiltersObj.at("platformIds"), JSON({ JSON("donkey"), JSON("billy") }));
    EXPECT_EQ(platformFiltersObj.at("platformVariants"), JSON({ JSON("RDK-STB"), JSON("RDK-PANEL") }));
    EXPECT_EQ(platformFiltersObj.at("yoctoVersions"), JSON({ JSON("KIRKSTONE") }));
    EXPECT_EQ(platformFiltersObj.at("countries"), JSON({ JSON("DEU"), JSON("AUT"), JSON("NZL") }));
    EXPECT_EQ(platformFiltersObj.at("subdivisions"), JSON({ JSON("GBR-SCT") }));
}

TEST(PackageMetaDataTest, testAllConfigs)
{
    Error error;

    const char *configXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<widget xmlns="http://www.bskyb.com/ns/widgets" version="2.0">
    <name short="allconfigs" version="1.2.3.4"> allconfigs </name>
    <icon src="icon.png" type="image/png"/>
    <content src="foo/html/index.html" type="application/html">
        <platformFilters>
        </platformFilters>
    </content>
    <capabilities>
        <capability name="lan-wan"/>
        <capability name="home-app"/>
        <capability name="firebolt"/>
        <capability name="start-timeout-sec"> 60</capability>
        <capability name="watchdog-timeout-sec">30 </capability>
        <capability name="issue-notifications"/>
        <capability name="block-notifications"/>
        <capability name="keymapping">Search, Volume-,    standby  </capability>
        <capability name="forward-keymapping">Search, Volume-,
            standby</capability>
        <capability name="storage"/>
        <capability name="private-storage">33 </capability>
        <capability name="https-mutual-authentication"/>
        <capability name="dial-app">example.com, Fred.co.uk</capability>
        <capability name="dial-id">foo,bar</capability>
        <capability name="dial-origin-mandatory"></capability>
        <capability name="preload"/>
        <capability name="local-services-3"/>
        <capability name="gesture-recogniser"/>
<!--
        <capability name="daemon-app"/>
        <capability name="system-app"/>
-->
        <capability name="hole-punch">
            1200, tcp:1234,
            udp:5670,
        </capability>
        <capability name="log-levels">default, milestone</capability>
        <capability name="stb-entitlements"/>
        <capability name="sound-scene">game</capability>
        <capability name="sound-mode">fruity</capability>
        <capability name="program-reference-level">9</capability>
        <capability name="suspend-mode"/>
        <capability name="hibernate-mode"/>
        <capability name="no-low-power-mode"/>
        <capability name="memory-intensive"/><capability name="refresh-rate-60hz"/>
        <capability name="drm-store"/>
        <capability name="drm-type">clearly</capability>
        <capability name="virtual-resolution"> 720p </capability>
        <capability name="forward-multicast">239.255.255.250:1900, 239.255.255.250:2500</capability>
        <capability name="pin-management">read-write</capability>
        <capability name="catalogue-id">some-catalogue-id</capability>
        <capability name="thunder"/>
        <capability name="multicast-server-socket">FRED:239.255.255.250:1900, TREVOR:239.255.255.250:2500</capability>
        <capability name="multicast-client-socket">CLIENT1, CLIENT2</capability>
        <capability name="fkps">file1.key,
            file2.soo   ,
            foo.bar</capability>
        <capability name="as-player"/>
        <capability name="mediarite-underlay"/>
        <capability name="mapi">Main:trusted;OrbDaemon:default,core</capability>
        <capability name="bearer-token-authentication"/>
        <capability name="sys-memory-limit">12m </capability>
        <capability name="gpu-memory-limit"> 12000k</capability>
        <capability name="rialto"/>
        <capability name="child-app">some.parent</capability>
        <capability name="age-rating">6</capability>
        <capability name="content-partner-id">dfgfgdfgdfg</capability>
        <capability name="game-controller"/>
        <capability name="airplay2"></capability>
        <capability name="pre-launch">Never</capability>
        <capability name="game-mode">dynamic</capability>
        <capability name="read-external-storage"></capability>
        <capability name="write-external-storage"></capability>
        <capability name="post-intents"></capability>
        <capability name="tsb-storage"></capability>
        <capability name="local-socket-server">12345,45678</capability>
        <capability name="local-socket-client">6788</capability>
        <capability name="pathway-app"></capability>
        <capability name="compositor-app"></capability>
        <capability name="chromecast"></capability>
        <capability name="age-policy">privacy:US:U13</capability>
        <capability name="intercept">true</capability>
     </capabilities>
    <parentalControl>true</parentalControl>
</widget>
    )";

    auto metaData = W3CPackageMetaDataImpl::fromConfigXml({ configXml, configXml + strlen(configXml) }, &error);
    EXPECT_FALSE(error) << error.what();
    ASSERT_NE(metaData, nullptr);

    TestPackageMetaData test(metaData);
    ASSERT_TRUE(test.isValid());
    ASSERT_FALSE(test.isNull());
    EXPECT_EQ(test.id(), "allconfigs");
    EXPECT_EQ(test.versionName(), "1.2.3.4");
    EXPECT_EQ(test.title(), " allconfigs ");
    EXPECT_EQ(test.mimeType(), "application/html");
    EXPECT_EQ(test.entryPointPath(), "foo/html/index.html");
    EXPECT_TRUE(metaData->entryArgs().empty());
    EXPECT_EQ(test.type(), PackageType::Application);
    ASSERT_TRUE(test.applicationInfo().has_value());

    EXPECT_EQ(test.overrides(Override::Application), JSON());
    EXPECT_EQ(test.overrides(Override::Runtime), JSON());
    EXPECT_EQ(test.overrides(Override::Base), JSON());

    EXPECT_TRUE(test.applicationInfo()->permissions().get(INTERNET_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(HOME_APP_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(COMPOSITOR_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(FIREBOLT_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(THUNDER_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(RIALTO_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(GAME_CONTROLLER_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(READ_EXTERNAL_STORAGE_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(WRITE_EXTERNAL_STORAGE_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(OVERLAY_PERMISSION));

    EXPECT_FALSE(test.applicationInfo()->permissions().get(ENTOS_AS_ACCESS_LEVEL1_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_AS_ACCESS_LEVEL3_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_MEMORY_INTENSIVE_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_AS_POST_INTENT_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_AS_PLAYER_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_CHROMECAST_PERMISSION));
    EXPECT_TRUE(test.applicationInfo()->permissions().get(ENTOS_AIRPLAY_PERMISSION));

    EXPECT_EQ(test.applicationInfo()->supportedLifecycleStates(),
              LifecycleStates::Paused | LifecycleStates::Suspended | LifecycleStates::Hibernated);

    const auto &dial = test.applicationInfo()->dialInfo();
    ASSERT_TRUE(dial.has_value());
    EXPECT_EQ(dial->corsDomains, std::vector<std::string>({ "example.com", "Fred.co.uk" }));
    EXPECT_EQ(dial->dialIds, std::vector<std::string>({ "foo", "bar" }));
    EXPECT_EQ(dial->originHeaderRequired, true);

    const auto &inputHandling = test.applicationInfo()->inputHandlingInfo();
    ASSERT_TRUE(inputHandling.has_value());
    EXPECT_EQ(inputHandling->interceptedKeys, std::vector<std::string>({}));
    EXPECT_EQ(inputHandling->capturedKeys, std::vector<std::string>({ "Search", "Volume-", "standby" }));
    EXPECT_EQ(inputHandling->monitoredKeys, std::vector<std::string>({ "Search", "Volume-", "standby" }));

    const auto &displayInfo = test.applicationInfo()->displayInfo();
    EXPECT_EQ(displayInfo.size, DisplaySize::Size1280x720);
    EXPECT_EQ(displayInfo.refreshRate, DisplayRefreshRate::SixtyHz);
    EXPECT_STRCASEEQ(displayInfo.pictureMode.value().c_str(), "GameModeDynamic");

    const auto &audioInfo = test.applicationInfo()->audioInfo();
    EXPECT_EQ(audioInfo.soundScene, "game");
    EXPECT_EQ(audioInfo.soundMode, "fruity");

    EXPECT_EQ(test.applicationInfo()->memoryQuota(), 12 * 1024 * 1024);
    EXPECT_EQ(test.applicationInfo()->gpuMemoryQuota(), 12000 * 1024);
    EXPECT_EQ(test.applicationInfo()->storageQuota(), 33 * 1024 * 1024);

    EXPECT_EQ(test.applicationInfo()->sharedStorageGroup(), "some.parent");

    EXPECT_EQ(test.applicationInfo()->watchdogInterval(), 30s);
    EXPECT_EQ(test.applicationInfo()->startTimeout(), 60s);

    EXPECT_EQ(test.applicationInfo()->publicServices(), std::vector<NetworkService>({
                                                            NetworkService{ "", NetworkService::TCP, 1200 },
                                                            NetworkService{ "", NetworkService::TCP, 1234 },
                                                            NetworkService{ "", NetworkService::UDP, 5670 },
                                                        }));
    EXPECT_EQ(test.applicationInfo()->exportedServices(), std::vector<NetworkService>({
                                                              NetworkService{ "", NetworkService::TCP, 12345 },
                                                              NetworkService{ "", NetworkService::TCP, 45678 },
                                                          }));
    EXPECT_EQ(test.applicationInfo()->importedServices(), std::vector<NetworkService>({
                                                              NetworkService{ "", NetworkService::TCP, 6788 },
                                                          }));

    EXPECT_EQ(test.applicationInfo()->loggingLevels(), LoggingLevels::Default | LoggingLevels::Milestone);

    EXPECT_EQ(test.vendorConfigKeys(),
              std::set<std::string>({ ENTOS_AGE_POLICY_CONFIGURATION, ENTOS_AGE_RATING_CONFIGURATION,
                                      ENTOS_MEDIARITE_CONFIGURATION, ENTOS_CONTENT_PARTNER_ID_CONFIGURATION,
                                      ENTOS_CATALOGUE_ID_CONFIGURATION, ENTOS_PIN_MANAGEMENT_CONFIGURATION,
                                      ENTOS_PRELAUNCH_CONFIGURATION, ENTOS_PARENTAL_CONTROL_CONFIGURATION,
                                      ENTOS_FKPS_CONFIGURATION, ENTOS_MULTICAST_CONFIGURATION,
                                      ENTOS_LEGACY_DRM_CONFIGURATION, ENTOS_MARKETPLACE_INTERCEPT_CONFIGURATION }));

    EXPECT_EQ(test.vendorConfig(ENTOS_AGE_POLICY_CONFIGURATION), JSON("privacy:US:U13"));
    EXPECT_EQ(test.vendorConfig(ENTOS_AGE_RATING_CONFIGURATION), JSON(6));

    EXPECT_EQ(test.vendorConfig(ENTOS_MEDIARITE_CONFIGURATION), strToJson(R"json({
        "accessGroups": {
            "Main": ["trusted"],
            "OrbDaemon": ["default", "core"]
        },
        "underlay": true
    })json"));

    EXPECT_EQ(test.vendorConfig(ENTOS_CONTENT_PARTNER_ID_CONFIGURATION), JSON("dfgfgdfgdfg"));
    EXPECT_EQ(test.vendorConfig(ENTOS_CATALOGUE_ID_CONFIGURATION), JSON("some-catalogue-id"));
    EXPECT_EQ(test.vendorConfig(ENTOS_PIN_MANAGEMENT_CONFIGURATION), JSON("readwrite"));
    EXPECT_EQ(test.vendorConfig(ENTOS_PRELAUNCH_CONFIGURATION), JSON("never"));
    EXPECT_EQ(test.vendorConfig(ENTOS_PARENTAL_CONTROL_CONFIGURATION), JSON(true));

    EXPECT_EQ(test.vendorConfig(ENTOS_MARKETPLACE_INTERCEPT_CONFIGURATION), strToJson(R"json({
    "enable": true
    })json"));

    EXPECT_EQ(test.vendorConfig(ENTOS_FKPS_CONFIGURATION), strToJson(R"json({
        "files": ["file1.key", "file2.soo", "foo.bar" ]
    })json"));

    EXPECT_EQ(test.vendorConfig(ENTOS_MULTICAST_CONFIGURATION), strToJson(R"json({
        "forwarding": [
            { "address": "239.255.255.250", "port": 1900 },
            { "address": "239.255.255.250", "port": 2500 }
        ],
        "clientSockets": [
            { "name": "CLIENT1" },
            { "name": "CLIENT2" }
        ],
        "serverSockets": [
            { "name": "FRED", "address": "239.255.255.250", "port": 1900 },
            { "name": "TREVOR", "address": "239.255.255.250", "port": 2500 }
        ]
    })json"));

    EXPECT_EQ(test.vendorConfig(ENTOS_LEGACY_DRM_CONFIGURATION), strToJson(R"json({
        "storageSize":  0,
        "types": [ "clearly" ]
    })json"));
}