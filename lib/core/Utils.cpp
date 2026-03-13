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

#include "Utils.h"
#include "LogMacros.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

bool verifyPackageId(const std::string &id)
{
    if (id.empty())
        return false;
    if (id.size() > 256)
        return false;

    // check the first character is alphanumeric
    auto it = id.begin();
    char ch = *it++;
    if (!isalnum(ch))
        return false;

    for (; it != id.end(); ++it)
    {
        char prev = ch;
        ch = *it;

        // check for double dots
        if (ch == '.')
        {
            if (prev == '.')
                return false;
        }
        else if (!isalnum(ch) && (ch != '-') && (ch != '_'))
        {
            return false;
        }
    }

    // check the last character is alphanumeric
    return isalnum(ch);
}

bool verifyPackagePath(const std::filesystem::path &path)
{
    // An empty path is invalid
    if (path.empty())
        return false;

    // An absolute path is also invalid
    if (path.is_absolute())
        return false;

    // Check for '..' in the path
    for (const auto &part : path)
    {
        if (part == "..")
            return false;
    }

    return true;
}

namespace LIBRALF_NS
{
    // NOLINTBEGIN(misc-no-recursion): Allow recursive JSON types

    void from_json(const nlohmann::json &json, JSONValue &value)
    {
        switch (json.type())
        {
            case nlohmann::json::value_t::null:
            case nlohmann::json::value_t::discarded:
                value = std::monostate();
                return;
            case nlohmann::json::value_t::boolean:
                value = json.get<bool>();
                return;
            case nlohmann::json::value_t::number_integer:
                value = json.get<int64_t>();
                return;
            case nlohmann::json::value_t::number_unsigned:
                value = static_cast<int64_t>(json.get<uint64_t>());
                return;
            case nlohmann::json::value_t::number_float:
                value = json.get<double>();
                return;
            case nlohmann::json::value_t::string:
                value = json.get<std::string>();
                return;
            case nlohmann::json::value_t::array:
                value = json.get<std::vector<JSON>>();
                return;
            case nlohmann::json::value_t::object:
                value = json.get<std::map<std::string, JSON>>();
                return;
            default:
                logWarning("Unsupported JSON type '%s'", json.type_name());
                value = std::monostate();
                return;
        }
    }

    void from_json(const nlohmann::json &json, JSON &value)
    {
        json.get_to<JSONValue>(value.value);
    }

    // NOLINTEND(misc-no-recursion)
} // namespace LIBRALF_NS

Result<ExtractOperation> checkExtraction(int targetDirFd, const std::filesystem::path &path,
                                         std::filesystem::file_type type, std::filesystem::perms perms,
                                         Package::ExtractOptions options)
{
    // Get the details on the existing file if it exists
    struct stat st = {};
    if (fstatat(targetDirFd, path.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0)
    {
        if (errno == ENOENT)
            return ExtractOperation::Extract;

        return Error::format(std::error_code(errno, std::system_category()), "Failed to stat '%s'", path.c_str());
    }

    if ((options & Package::ExtractOption::KeepExistingFiles) == Package::ExtractOption::KeepExistingFiles)
        return Error::format(ErrorCode::FileExists, "File '%s' already exists in output directory", path.c_str());

    // Check if we can skip existing files, however note that it's an error if the entry is a directory and
    // we're not writing a directory
    if ((options & Package::ExtractOption::SkipExistingFiles) == Package::ExtractOption::SkipExistingFiles)
    {
        if ((type == std::filesystem::file_type::directory && !S_ISDIR(st.st_mode)) ||
            (type != std::filesystem::file_type::directory && S_ISREG(st.st_mode)))
        {
            return Error::format(ErrorCode::FileExists, "Cannot skip existing '%s' as mismatched types", path.c_str());
        }

        return ExtractOperation::Skip;
    }

    // Now can unlink the existing file or directory
    if (S_ISDIR(st.st_mode))
    {
        if (type == std::filesystem::file_type::directory)
        {
            if ((options & Package::ExtractOption::NoOverwriteDirectories) !=
                Package::ExtractOption::NoOverwriteDirectories)
            {
                if (fchmodat(targetDirFd, path.c_str(), modeFromFsPerms(perms), 0) != 0)
                {
                    return Error::format(std::error_code(errno, std::system_category()),
                                         "Failed to update permissions of directory '%s'", path.c_str());
                }
            }

            // Returning false means skip creating the directory
            return ExtractOperation::Skip;
        }
        else if (unlinkat(targetDirFd, path.c_str(), AT_REMOVEDIR) != 0)
        {
            // We're trying to write a regular file where a directory was, so we tried removing it and this failed,
            // probably because it's not empty, so this is an error and breaks extraction
            return Error::format(ErrorCode::FileExists, "Cannot overwrite directory '%s' with a file", path.c_str());
        }
        else
        {
            return ExtractOperation::Extract;
        }
    }
    else
    {
        // Unlink the old file or symlink, ready for replacing with new version
        if (unlinkat(targetDirFd, path.c_str(), 0) != 0)
        {
            return Error::format(std::error_code(errno, std::system_category()), "Failed to unlink '%s'", path.c_str());
        }

        return ExtractOperation::Extract;
    }
}
