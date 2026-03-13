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

#include "PackageReader.h"
#include "IPackageEntryImpl.h"
#include "IPackageImpl.h"
#include "IPackageReaderImpl.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

// -------------------------------------------------------------------------
/*!
    \class ErroredPackageReader
    \brief Basic reader implementation created when a package couldn't be
    read.

 */
class ErroredPackageReader : public LIBRALF_NS::IPackageReaderImpl
{
public:
    explicit ErroredPackageReader(Error error)
        : m_error(std::move(error))
    {
    }

    ~ErroredPackageReader() override = default;

    std::unique_ptr<IPackageEntryImpl> next() override { return nullptr; }

    bool hasError() const override { return true; }

    Error error() const override { return m_error; }

private:
    Error m_error;
};

PackageReader PackageReader::create(const Package &package)
{
    Error readerError;

    auto reader = package.m_impl->createReader(&readerError);
    if (reader)
        return PackageReader(std::move(reader));
    else
        return PackageReader(std::make_unique<ErroredPackageReader>(readerError));
}

PackageReader::PackageReader(std::unique_ptr<IPackageReaderImpl> &&impl)
    : m_impl(std::move(impl))
{
}

PackageReader::~PackageReader() // NOLINT(modernize-use-equals-default)
{
}

std::optional<PackageEntry> PackageReader::next()
{
    auto entryImpl = m_impl->next();
    if (!entryImpl)
        return std::nullopt;

    return PackageEntry(std::move(entryImpl));
}

bool PackageReader::hasError() const
{
    return m_impl->hasError();
}

Error PackageReader::error() const
{
    return m_impl->error();
}
