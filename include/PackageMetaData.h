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

#include "EnumFlags.h"
#include "LibRalf.h"
#include "VersionConstraint.h"
#include "VersionNumber.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

// -------------------------------------------------------------------------
/*!
    \file PackageMetaData.h

    The objects for reading the package meta-data and privileges.

 */

// -------------------------------------------------------------------------
/*!
    \defgroup Permissions
    \brief Permissions that an application or service can request.

    \{
 */

// -------------------------------------------------------------------------
/*!
    The app or service requires access to the network.
 */
#define INTERNET_PERMISSION "urn:rdk:permission:internet"

// -------------------------------------------------------------------------
/*!
    The application is requesting to be the home / launcher app.  An app
    with this privilege will be started when the device boots and will
    be the app that is shown when the user presses the home button.

    This is typically combined with the \c DisplayOverlays and \c Compositor
    privileges to allow the app to draw the home screen and control
    UI on the device.
 */
#define HOME_APP_PERMISSION "urn:rdk:permission:home-app"

// -------------------------------------------------------------------------
/*!
    The app or service requires access to the firebolt API.  This is just
    used to request access to firebolt in general, additional firebolt
    specific privileges may be required to access specific firebolt APIs.
 */
#define FIREBOLT_PERMISSION "urn:rdk:permission:firebolt"

// -------------------------------------------------------------------------
/*!
    The app or service requires access to the thunder API.  This is just
    used to request access to thunder in general, additional thunder
    specific privileges may be required to access specific thunder APIs.
 */
#define THUNDER_PERMISSION "urn:rdk:permission:thunder"

// -------------------------------------------------------------------------
/*!
    The app or service requires access to the Rialto API for A/V playback.
 */
#define RIALTO_PERMISSION "urn:rdk:permission:rialto"

// -------------------------------------------------------------------------
/*!
    The app is requesting access to any connected game controller devices.
 */
#define GAME_CONTROLLER_PERMISSION "urn:rdk:permission:game-controller"

// -------------------------------------------------------------------------
/*!
    The app or service is requesting access to be able to read data from
    attached external storage devices.  _External storage devices_ are
    typically USB memory sticks.
 */
#define READ_EXTERNAL_STORAGE_PERMISSION "urn:rdk:permission:external-storage:read"

// -------------------------------------------------------------------------
/*!
    The app or service is requesting access to be able to read and write data
    from / to attached external storage devices.  _External storage devices_
    are typically USB memory sticks.
 */
#define WRITE_EXTERNAL_STORAGE_PERMISSION "urn:rdk:permission:external-storage:write"

// -------------------------------------------------------------------------
/*!
    The app or service is requesting privilege to display overlays on the
    screen.  Overlays are typically popups or other UI elements that are
    drawn on top of the normal application UI.
    */
#define OVERLAY_PERMISSION "urn:entos:permission:display-overlay"

// -------------------------------------------------------------------------
/*!
    The application is requesting access to the composition API.  This allows
    the app to control the layout and composition of the screen for all apps.
    In this sense it acts like a basic window manager and is typically used
    in conjunction with the \c HomeApp privilege to allow the app to act
    like a launcher or desktop app.

 */
#define COMPOSITOR_PERMISSION "urn:rdk:permission:compositor"

// -------------------------------------------------------------------------
/*!
    The app or service is requesting access to the time shift buffer, used
    for pausing and rewinding video.

*/
#define TIME_SHIFT_BUFFER_PERMISSION "urn:rdk:permission:timeshift-buffer"

// -------------------------------------------------------------------------
/*!
    \}

*/

namespace LIBRALF_NS
{
    class IPermissionsImpl;
    class IPackageMetaDataImpl;

    // -------------------------------------------------------------------------
    /*!
        \struct JSON

        Basic structure to represent a JSON value.  This is used to returned
        parsed JSON vendor data from the package meta-data.

        This is light weight and does not support all JSON features, it is intended
        to be used for simple JSON structures that are typically used in package
        meta-data.  It supports null, boolean, number, string, array and object
        types.

        \warning The asXXXX() methods will throw an exception if the value is not
        of the expected type.

    */

    // NOLINTBEGIN(misc-no-recursion): Allow recursive JSON types

    struct JSON;

    using JSONValue = std::variant<std::monostate,             ///< Represents a JSON null value
                                   bool,                       ///< Represents a JSON boolean value
                                   int64_t,                    ///< Represents a JSON number value
                                   double,                     ///< Represents a JSON number value
                                   std::string,                ///< Represents a JSON string value
                                   std::vector<JSON>,          ///< Represents a JSON array value
                                   std::map<std::string, JSON> ///< Represents a JSON object value
                                   >;

    struct JSON
    {
        JSONValue value;

        JSON()
            : value(std::monostate())
        {
        }
        explicit JSON(bool v)
            : value(v)
        {
        }
        explicit JSON(int v)
            : value(static_cast<int64_t>(v))
        {
        }
        explicit JSON(unsigned v)
            : value(static_cast<int64_t>(v))
        {
        }
        explicit JSON(std::int64_t v)
            : value(v)
        {
        }
        explicit JSON(std::uint64_t v)
            : value(static_cast<int64_t>(v))
        {
        }
        explicit JSON(double v)
            : value(v)
        {
        }
        explicit JSON(float v)
            : value(static_cast<double>(v))
        {
        }
        explicit JSON(std::string v)
            : value(std::move(v))
        {
        }
        explicit JSON(std::string_view v)
            : value(std::string(v))
        {
        }
        explicit JSON(const char *v)
            : value(std::string(v))
        {
        }
        explicit JSON(std::vector<JSON> v)
            : value(std::move(v))
        {
        }
        JSON(std::initializer_list<JSON> v)
            : value(std::vector(v))
        {
        }
        explicit JSON(std::map<std::string, JSON> v)
            : value(std::move(v))
        {
        }
        JSON(std::initializer_list<std::pair<const char *, JSON>> v)
        {
            std::map<std::string, JSON> value_;
            for (auto &pair : v)
                value_.emplace(pair.first, pair.second);

            value = std::move(value_);
        }

        inline bool isNull() const { return std::holds_alternative<std::monostate>(value); }
        inline bool isBool() const { return std::holds_alternative<bool>(value); }
        inline bool isInteger() const { return std::holds_alternative<int64_t>(value); }
        inline bool isDouble() const { return std::holds_alternative<double>(value); }
        inline bool isString() const { return std::holds_alternative<std::string>(value); }
        inline bool isArray() const { return std::holds_alternative<std::vector<JSON>>(value); }
        inline bool isObject() const { return std::holds_alternative<std::map<std::string, JSON>>(value); }

        inline bool asBool() const { return std::get<bool>(value); }
        inline int64_t asInteger() const { return std::get<int64_t>(value); }
        inline double asDouble() const { return std::get<double>(value); }
        inline const std::string &asString() const { return std::get<std::string>(value); }
        inline const std::vector<JSON> &asArray() const { return std::get<std::vector<JSON>>(value); }
        inline const std::map<std::string, JSON> &asObject() const
        {
            return std::get<std::map<std::string, JSON>>(value);
        }

        inline std::vector<JSON> &asArray() { return std::get<std::vector<JSON>>(value); }
        inline std::map<std::string, JSON> &asObject() { return std::get<std::map<std::string, JSON>>(value); }
    };

    inline bool operator==(const JSON &lhs, const JSON &rhs)
    {
        return (lhs.value == rhs.value);
    }

    inline bool operator!=(const JSON &lhs, const JSON &rhs)
    {
        return (lhs.value != rhs.value);
    }

    LIBRALF_EXPORT std::ostream &operator<<(std::ostream &s, const JSON &j);

    // NOLINTEND(misc-no-recursion)

    // -------------------------------------------------------------------------
    /*!
        \class Permissions
        \brief A set of permissions that an app or service can request.

    */
    class LIBRALF_EXPORT Permissions
    {
    public:
        Permissions() = default;
        Permissions(const Permissions &other) = default;
        Permissions(Permissions &&other) noexcept = default;
        ~Permissions() = default;

        Permissions &operator=(const Permissions &other) = default;
        Permissions &operator=(Permissions &&other) noexcept = default;

        // -------------------------------------------------------------------------
        /*!
            Returns the value of the given privilege.
         */
        bool get(std::string_view privilege) const;

        // -------------------------------------------------------------------------
        /*!
            Returns the value of the given privilege.
         */
        bool operator[](std::string_view privilege) const;

        // -------------------------------------------------------------------------
        /*!
            Returns all the privileges requested by the app.
         */
        std::set<std::string> all() const;

    protected:
        friend class ApplicationAndServiceInfo;
        explicit Permissions(std::shared_ptr<IPermissionsImpl> impl);

    private:
        std::shared_ptr<IPermissionsImpl> m_impl;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct LifecycleStates
        \brief Set of flags describing the supported lifecycle states of an app.

        \var LifecycleStates::Paused The app supports going into a background
        state.

        \var LifecycleStates::Suspended The app supports going into the suspended
        state.

        \var LifecycleStates::Hibernated The app supports being hibernated.

        \var LifecycleStates::LowPower The app is allowed to remain running when
        the device enters a low power state.
     */
    enum class LifecycleState
    {
        Paused = (1 << 0),
        Suspended = (1 << 1),
        Hibernated = (1 << 2),
        LowPower = (1 << 3),
    };

    LIBRALF_ENUM_FLAGS(LifecycleStates, LifecycleState)

    // -------------------------------------------------------------------------
    /*!
        \struct LoggingLevels
        \brief Set of flags for the logging levels the app would like logged.

     */
    enum class LoggingLevel
    {
        Default = (1 << 0),
        Debug = (1 << 1),
        Info = (1 << 2),
        Milestone = (1 << 3),
        Warning = (1 << 4),
        Error = (1 << 5),
        Fatal = (1 << 6),
    };

    LIBRALF_ENUM_FLAGS(LoggingLevels, LoggingLevel)

    // -------------------------------------------------------------------------
    /*!
        \struct NetworkService
        \brief Describes the network service that an app or service package
        wants to expose, export or import.

        \var NetworkService::name The name of the service, this is intended to be
        a human readable name for the service, however currently it's not
        populated.

        \var NetworkService::protocol The protocol that the service uses, either
        TCP or UDP.

        \var NetworkService::port The port that the service listens on or connects
        to.
     */
    struct NetworkService
    {
        std::string name;
        enum Protocol : uint32_t
        {
            TCP,
            UDP
        } protocol = TCP;
        uint16_t port = 0;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct DialInfo
        \brief Information about the DIAL capabilities of an application.

        \var DialInfo::dialIds The list of DIAL ids that the app supports.

        \var DialInfo::corsDomains The list of CORS domains that the app will
        allow to control the app via the DIAL service.

        \var DialInfo::originHeaderRequired If \c true then the app requires
        that the Origin header is present in the DIAL request.  Prior to DIAL
        version 2.1.1 the Origin header was optional.
     */
    struct DialInfo
    {
        std::vector<std::string> dialIds;
        std::vector<std::string> corsDomains;
        bool originHeaderRequired = false;
    };

    // -------------------------------------------------------------------------
    /*!
        \enum DisplaySize
        \brief Possible display sizes that an app can request.
     */
    enum class DisplaySize : uint32_t
    {
        Default = 0,
        Size720x480,
        Size720x576,
        Size1280x720,
        Size1920x1080,
        Size3840x2160,
        Size7680x4320,
    };

    // -------------------------------------------------------------------------
    /*!
        \enum DisplaySize
        \brief Possible display refresh rates that an app can request.
     */
    enum class DisplayRefreshRate : uint32_t
    {
        Default = 0,
        FiftyHz,
        SixtyHz,
    };

    // -------------------------------------------------------------------------
    /*!
        \struct DisplayInfo
        \brief Possible display refresh rates that an app can request.

        \var DisplayInfo::size The display size requested by the app.

        \var DisplayInfo::refreshRate The display refresh rate requested by the app.
     */
    struct DisplayInfo
    {
        DisplaySize size = DisplaySize::Default;
        DisplayRefreshRate refreshRate = DisplayRefreshRate::Default;
        std::optional<std::string> pictureMode;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct AudioInfo
        \brief Possible audio settings that an app can request.

        \var AudioInfo::soundMode The optional name of the sound mode to set.

        \var AudioInfo::soundScene The optional name of the sound scene to set.

        \var AudioInfo::loudnessAdjustment Loudness adjustment value for the app,
     */
    struct AudioInfo
    {
        std::optional<std::string> soundMode;
        std::optional<std::string> soundScene;
        std::optional<int32_t> loudnessAdjustment;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct InputHandlingInfo
        \brief Describes the input handling requirements of an app.

        \var InputHandlingInfo::capturedKeys The set of Input keys that an app
        always receives when it has focus, overriding any intercept keys from other
        apps, and preventing those keys from being forwarded to other apps.

        \var InputHandlingInfo::interceptedKeys The set of 'system' keys that the
        app wants receive even when it doesn't have input focus.  The app will
        receive key and it won't be forwarded to the currently focused app.

        \var InputHandlingInfo::monitoredKeys Similar to the \c interceptedKeys,
        however the input event will also be forwarded to the focus app.
     */
    struct InputHandlingInfo
    {
        std::vector<std::string> interceptedKeys;
        std::vector<std::string> capturedKeys;
        std::vector<std::string> monitoredKeys;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct Icon
        \brief Structure to hold the icon information for the package.

        The package can contain multiple icons, each with a different type, sizes
        and purpose.

     */
    struct Icon
    {
        std::filesystem::path path;
        std::string mimeType;
        std::string purpose;
        std::vector<std::pair<int, int>> sizes;
    };

    // -------------------------------------------------------------------------
    /*!
        \struct Icon
        \brief Structure to hold the icon information for the package.

        The package can contain multiple icons, each with a different type, sizes
        and purpose.

     */
    struct PlatformInfo
    {
        std::optional<std::string> architecture;
        std::optional<std::string> variant;
        std::optional<std::string> os;
    };

    // -------------------------------------------------------------------------
    /*!
        \class RuntimeInfo
        \brief The runtime information from the package meta data.

        If the package contains an runtime then this class provides the
        information about the runtime.
     */
    class LIBRALF_EXPORT RuntimeInfo
    {
    public:
        RuntimeInfo(const RuntimeInfo &other) = default;
        RuntimeInfo(RuntimeInfo &&other) noexcept = default;
        ~RuntimeInfo() = default;

        RuntimeInfo &operator=(const RuntimeInfo &other) = default;
        RuntimeInfo &operator=(RuntimeInfo &&other) noexcept = default;

    private:
        friend class PackageMetaData;

        explicit RuntimeInfo(std::shared_ptr<IPackageMetaDataImpl> impl);

        std::shared_ptr<IPackageMetaDataImpl> m_impl;
    };

    // -------------------------------------------------------------------------
    /*!
        \class ApplicationAndServiceInfo
        \brief Common information for applications and services from the package.

        Don't use this class directly, use either ApplicationInfo or ServiceInfo.
     */
    class LIBRALF_EXPORT ApplicationAndServiceInfo
    {
    public:
        virtual ~ApplicationAndServiceInfo() = default;

    public:
        // -------------------------------------------------------------------------
        /*!
            Optional "runtime" type of the application.  If present this can be
            used to help determine the type of runtime that the app requires to run.

            If not present in the package metadata then an empty string is returned.
         */
        const std::string &runtimeType() const;

        // -------------------------------------------------------------------------
        /*!
            The amount of persistent storage, in bytes, that the app has requested.
            If the app has not requested any storage then this will return
            std::nullopt.
         */
        std::optional<uint64_t> storageQuota() const;

        // -------------------------------------------------------------------------
        /*!
            If the application or services wants to use the storage of another
            app (or service), then this returns the group / id of that app.

            If the app doesn't want to share its storage then this will return std::nullopt.
         */
        const std::optional<std::string> &sharedStorageGroup() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the requested memory quota, in bytes, for the app or service.
         */
        std::optional<uint64_t> memoryQuota() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the set of permissions that the app package has requested.

            \see Permissions
         */
        Permissions permissions() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the set of lifecycle states that the app or service supports.

            For example an app may support being in the LifecycleState::Inactive state,
            but not LifecycleState::Hibernated.
         */
        LifecycleStates supportedLifecycleStates() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the set of services that the app package wants to expose outside
            the device, ie. on the local network.

            This has traditionally been called 'hole-punch', as typically this
            capability is used to punch a hole in the app sandbox to allow external
            access to the app on given port / protocol.
         */
        const std::vector<NetworkService> &publicServices() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the set of services that the app package wants to expose to other
            apps running on the device.  These services are not exposed outside the
            device.

            Other apps that want to use these services must declare that they want
            to in their own importedServices() list.
         */
        const std::vector<NetworkService> &exportedServices() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the set of services that the app package wants to be able to
            connect to on the device.
         */
        const std::vector<NetworkService> &importedServices() const;

        // -------------------------------------------------------------------------
        /*!
            Optional start-up timeout value.  If the app or service hasn't notified
            the system it is running before the timeout it may be killed.
         */
        const std::optional<std::chrono::milliseconds> &startTimeout() const;

        // -------------------------------------------------------------------------
        /*!
            Optional watchdog timeout value.
         */
        const std::optional<std::chrono::milliseconds> &watchdogInterval() const;

        // -------------------------------------------------------------------------
        /*!
            Returns a set of logging levels that the app would like logged and
            uploaded.

         */
        LoggingLevels loggingLevels() const;

    protected:
        explicit ApplicationAndServiceInfo(std::shared_ptr<IPackageMetaDataImpl> impl);

        std::shared_ptr<IPackageMetaDataImpl> m_impl;
    };

    // -------------------------------------------------------------------------
    /*!
        \class ApplicationInfo
        \brief The application information from the package meta data.

        If the package contains an application then this class provides the
        information about the application.
     */
    class LIBRALF_EXPORT ApplicationInfo final : public ApplicationAndServiceInfo
    {
    public:
        ApplicationInfo(const ApplicationInfo &other) = default;
        ApplicationInfo(ApplicationInfo &&other) noexcept = default;
        ~ApplicationInfo() final = default;

        ApplicationInfo &operator=(const ApplicationInfo &other) = default;
        ApplicationInfo &operator=(ApplicationInfo &&other) noexcept = default;

    public:
        // -------------------------------------------------------------------------
        /*!
            If the app supports DIAL, then this will return the DIAL info.
         */
        const std::optional<DialInfo> &dialInfo() const;

        // -------------------------------------------------------------------------
        /*!
            The set of 'system' keys that the app wants to capture.

            This returns the key names as specified in the package, it's up to the
            caller to determine the correct key codes for the system.
         */
        const std::optional<InputHandlingInfo> &inputHandlingInfo() const;

        // -------------------------------------------------------------------------
        /*!
            The requested display details for the app.  Apps can request different
            display sizes and refresh rates to best match their content.
         */
        const DisplayInfo &displayInfo() const;

        // -------------------------------------------------------------------------
        /*!
            The requested audio details for the app.  Apps can request different
            audio settings to best match their content.
         */
        const AudioInfo &audioInfo() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the requested GPU memory quota, in bytes, for the app or service.
         */
        std::optional<uint64_t> gpuMemoryQuota() const;

    private:
        friend class PackageMetaData;

        explicit ApplicationInfo(std::shared_ptr<IPackageMetaDataImpl> impl);
    };

    // -------------------------------------------------------------------------
    /*!
        \class ServiceInfo
        \brief The service information from the package meta data.

        If the package contains a service (aka system application, aka daemon
        application) then this class provides the information about the service.

        Services and Applications are similar in many ways, and many of the
        same fields as ApplicationInfo are present in ServiceInfo.  However
        things like DIAL and captured keys are not relevant to services.
    */
    class LIBRALF_EXPORT ServiceInfo final : public ApplicationAndServiceInfo
    {
    public:
        ServiceInfo(const ServiceInfo &other) = default;
        ServiceInfo(ServiceInfo &&other) noexcept = default;
        ~ServiceInfo() final = default;

        ServiceInfo &operator=(const ServiceInfo &other) = default;
        ServiceInfo &operator=(ServiceInfo &&other) noexcept = default;

    private:
        friend class PackageMetaData;

        explicit ServiceInfo(std::shared_ptr<IPackageMetaDataImpl> impl);
    };

    // -------------------------------------------------------------------------
    /*!
        The type of package, ie. whether it's a runtime, application or service.

     */
    enum class PackageType : uint32_t
    {
        Unknown,
        Runtime,
        Application,
        Service,
        Base,
        Resource
    };

    // -------------------------------------------------------------------------
    /*!
        The type of override you want to obtain from the package meta-data.

        \see PackageMetaData::overrides().
        \since 1.0.5
     */
    enum class Override : uint32_t
    {
        Application,
        Runtime,
        Base
    };

    // -------------------------------------------------------------------------
    /*!
        \class PackageMetaData
        \brief A bundle of metadata key value pairs.

        Stores the metadata stored within a package file.

     */
    class LIBRALF_EXPORT PackageMetaData
    {
    public:
        // -------------------------------------------------------------------------
        /*!
            Constructs a new, null, package metadata object.
        */
        PackageMetaData();

        PackageMetaData(const PackageMetaData &other);
        PackageMetaData(PackageMetaData &&other) noexcept;
        ~PackageMetaData();

        PackageMetaData &operator=(PackageMetaData &&other) noexcept;
        PackageMetaData &operator=(const PackageMetaData &other);

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if the package metadata is valid, ie. it was successfully
            verified and parsed from the package file.

            This is the opposite of isNull().
        */
        bool isValid() const;

        // -------------------------------------------------------------------------
        /*!
            Returns \c true if the package metadata is not valid, ie. there was an
            error verifying and parsing the metadata from the package file.

            This is the opposite of isValid().
        */
        bool isNull() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the id of the package, for apps this is typically referred to as
            the appId.

            If the package is valid then this is guaranteed to return a non-empty.
            If the package or it's metadata is invalid then this will return an
            empty string.
         */
        const std::string &id() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the semantic version of the package.  This is a strict semantic
            version number with a maximum of 4 fields.

            \note W3C style widgets don't use semantic versioning, and instead for
            those you should use the versionName() method.
         */
        VersionNumber version() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the version name string of the package.  The version name is free
            form and can be anything, including empty.

            If a version name was not present in the package then this will return
            the version() number as a string.
        */
        const std::string &versionName() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the type of the package, ie. whether it's a runtime, application
            or service.

            \see applicationInfo(), serviceInfo(), runtimeInfo().
        */
        PackageType type() const;

        // -------------------------------------------------------------------------
        /*!
            Returns details on the platform the package is intended to run on, if
            present in the package metadata.

         */
        const PlatformInfo &platformInfo() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the "mime" type of the package, this is typically a string like
            "application/html" or "runtime/flutter".  The leading part of this
            string is the package type, and the trailing part is the package subtype.

            For OCI packages this is combination of the `packageType` field and the
            `packageSpecifier` field in the package metadata, example:
                <packageType>/<packageSpecifier>

            If the `packageSpecifier` field is not present in the package metadata
            then this will return just the `<packageType>/unknown`.

            \see type().
        */
        const std::string &mimeType() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the optional title of the package.  This is seldom used, but
            can be a more human friendly name for the package.

            If the title is not present in the metadata then this will return an
            \c std::nullopt.
        */
        const std::optional<std::string> &title() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the entry point path for the package.  Every package has an
            entry point, but the interpretation of this path is up to the system
            that is using the package.

            Typically for runtime packages this will be the path to the init
            executable or script to run.  For app and service packages this path
            may be passed to the runtime init to start the app or service.
        */
        const std::filesystem::path &entryPointPath() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the list of dependencies for the package. A dependency is a
            package id and a version constraint.

        */
        const std::map<std::string, VersionConstraint> &dependencies() const;

        // -------------------------------------------------------------------------
        /*!
            Details on the icon files within the package.  The package meta data
            can list multiple icons with different types, sizes and purposes.

            If the package does not contain an icon then this will return
            an empty list.
         */
        const std::list<Icon> &icons() const;

        // -------------------------------------------------------------------------
        /*!
            If the package contains an application then this will return the details
            for the application.

            \c std::nullopt is returned if the package is not an application or there
            was an error reading the application info.
         */
        std::optional<ApplicationInfo> applicationInfo() const;

        // -------------------------------------------------------------------------
        /*!
            If the package contains an service (aka system app, aka daemon app) then
            this will return the details for the service.

            \c std::nullopt is returned if the package is not an service or there
            was an error reading the service info.
         */
        std::optional<ServiceInfo> serviceInfo() const;

        // -------------------------------------------------------------------------
        /*!
            If the package contains an runtime then this will return the details
            for the runtime.

            \c std::nullopt is returned if the package is not an application or there
            was an error reading the runtime info.
         */
        std::optional<RuntimeInfo> runtimeInfo() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the JSON parsed vendor setting for the given key.  If the
            setting is not present in the package meta data then a null JSON will
            be returned.

            \code
                auto setting = metaData.vendorSetting("urn:vendor:setting:foo");
                assert(setting.isString());
                assert(setting.asString() == "bar");
            \endcode
         */
        JSON vendorConfig(std::string_view key) const;

        // -------------------------------------------------------------------------
        /*!
            Returns the list of all vendor configuration keys that are present
            in the package meta data.  This is a list of all keys that can be
            queried using vendorConfig().

         */
        std::set<std::string> vendorConfigKeys() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the optional list of entry arguments for the package.  These are
            arguments that should be passed to the package entry point when it is
            started.

            \since 1.0.5
        */
        const std::list<std::string> &entryArgs() const;

        // -------------------------------------------------------------------------
        /*!
            Returns the override settings from the package meta data.  These are
            JSON blobs that may contain settings that should override the default
            settings.  There is no defined structure for these JSON blobs, it's up
            to the system using the package to interpret them.

            A package may contain multiple override sections, for different types
            of package.  For example an application package may contain overrides
            for the application itself, but also for the runtime that the app
            runs in.

            \since 1.0.5
         */
        JSON overrides(Override type) const;

    protected:
        friend class Package;

        explicit PackageMetaData(std::shared_ptr<IPackageMetaDataImpl> impl);

    private:
        std::shared_ptr<IPackageMetaDataImpl> m_impl;
    };

} // namespace LIBRALF_NS
