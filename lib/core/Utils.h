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
#include "Package.h"
#include "PackageMetaData.h"

#include <nlohmann/json.hpp>

#include <filesystem>

#include <fcntl.h>

// -------------------------------------------------------------------------
/*!
    \internal

    Checks the given \a id is a valid package id.  The rules for a package id
    are:
        - cannot be an empty string
        - cannot be longer than 256 characters
        - may only contain dots (.), dashes (-), underscore (_) and alphanumeric characters
        - in additional the following restrictions apply:
            - the first and last characters must be alphanumeric
            - double dots (..) are not allowed
 */
bool verifyPackageId(const std::string &id);

// -------------------------------------------------------------------------
/*!
    \internal

    Checks the given \a path to ensure it is safe to use.  The path must not
    be absolute, must not contain ".." and must not be empty.

 */
bool verifyPackagePath(const std::filesystem::path &path);

// -------------------------------------------------------------------------
/*!
    \internal

    Helper function to calculate the length of a string at compile time.

*/
constexpr std::size_t compileTimeStrLen(const char *_Nonnull str)
{
    std::size_t length = 0;
    while (str[length] != '\0')
        ++length;
    return length;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Helper to convert posix mode_t to a std::filesystem::perms object.

    \note This only converts the permissions bits and does not include
    special bits like setuid, setgid, or sticky bit.
*/
constexpr std::filesystem::perms fsPermsFromMode(const mode_t mode)
{
    std::filesystem::perms perms = std::filesystem::perms::none;

    if (mode & S_IRUSR)
        perms |= std::filesystem::perms::owner_read;
    if (mode & S_IWUSR)
        perms |= std::filesystem::perms::owner_write;
    if (mode & S_IXUSR)
        perms |= std::filesystem::perms::owner_exec;

    if (mode & S_IRGRP)
        perms |= std::filesystem::perms::group_read;
    if (mode & S_IWGRP)
        perms |= std::filesystem::perms::group_write;
    if (mode & S_IXGRP)
        perms |= std::filesystem::perms::group_exec;

    if (mode & S_IROTH)
        perms |= std::filesystem::perms::others_read;
    if (mode & S_IWOTH)
        perms |= std::filesystem::perms::others_write;
    if (mode & S_IXOTH)
        perms |= std::filesystem::perms::others_exec;

    return perms;
}

// -------------------------------------------------------------------------
/*!
    \internal

    Helper to convert std::filesystem::perms object to posix mode_t.

    \note This only converts the permissions bits and does not include
    special bits like setuid, setgid, or sticky bit.
 */
constexpr mode_t modeFromFsPerms(const std::filesystem::perms &perms)
{
    mode_t mode = 0;

    // Owner permissions
    if ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none)
        mode |= S_IRUSR;
    if ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none)
        mode |= S_IWUSR;
    if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none)
        mode |= S_IXUSR;

    // Group permissions
    if ((perms & std::filesystem::perms::group_read) != std::filesystem::perms::none)
        mode |= S_IRGRP;
    if ((perms & std::filesystem::perms::group_write) != std::filesystem::perms::none)
        mode |= S_IWGRP;
    if ((perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none)
        mode |= S_IXGRP;

    // Others permissions
    if ((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none)
        mode |= S_IROTH;
    if ((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none)
        mode |= S_IWOTH;
    if ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
        mode |= S_IXOTH;

    return mode;
}

namespace LIBRALF_NS
{
    // -------------------------------------------------------------------------
    /*!
        Helper functions to convert nlohmann::json values to our simplified
        JSON and JSONValue types.

        With these it means you can do the following to convert the value:
        \code{.cpp}
            nlohmann::json json = ...;
            LIBRALF_NS::JSONValue value = json.get<JSON>();
        \endcode

     */
    void from_json(const nlohmann::json &json, JSON &value);

    void from_json(const nlohmann::json &json, JSONValue &value);
} // namespace LIBRALF_NS

/// The operation to perform when extracting a package entry.
enum class ExtractOperation
{
    Skip,
    Extract
};

// -------------------------------------------------------------------------
/*!
    Checks if a package entry can be extracted to the given directory.
    This checks if the file already exists at the target and if so what it
    should do about it based on the supplied options.

    This is used by implementations of IPackageEntryImpl::writeTo() to decide
    if the entry should be extracted, skipped or if an error should be returned.

 */
LIBRALF_NS::Result<ExtractOperation> checkExtraction(int targetDirFd, const std::filesystem::path &path,
                                                     std::filesystem::file_type type, std::filesystem::perms perms,
                                                     LIBRALF_NS::Package::ExtractOptions options);
