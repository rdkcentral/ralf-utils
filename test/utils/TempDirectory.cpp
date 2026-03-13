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

#include "TempDirectory.h"
#include <chrono>
#include <random>

TempDirectory::TempDirectory()
    : m_autoRemove(true)
{
    // Use system temp directory and generate a unique name
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);

    std::string dirname = "tempdir_" + std::to_string(now) + "_" + std::to_string(dis(gen));
    m_path = std::filesystem::temp_directory_path() / dirname;

    std::filesystem::create_directory(m_path);
}

TempDirectory::TempDirectory(const std::filesystem::path &templatePath)
    : m_autoRemove(true)
{
    if (templatePath.is_absolute())
        m_path = templatePath;
    else
        m_path = std::filesystem::temp_directory_path() / templatePath;

    std::filesystem::create_directory(m_path);
}

TempDirectory::~TempDirectory()
{
    if (m_autoRemove)
    {
        remove();
    }
}

std::filesystem::path TempDirectory::path() const
{
    return m_path;
}

void TempDirectory::remove()
{
    if (std::filesystem::exists(m_path))
    {
        std::filesystem::remove_all(m_path);
    }
}

bool TempDirectory::autoRemove() const
{
    return m_autoRemove;
}

void TempDirectory::setAutoRemove(bool enable)
{
    m_autoRemove = enable;
}

void TempDirectory::removeContents()
{
    if (std::filesystem::exists(m_path) && std::filesystem::is_directory(m_path))
    {
        for (const auto &entry : std::filesystem::directory_iterator(m_path))
        {
            std::filesystem::remove_all(entry);
        }
    }
}
