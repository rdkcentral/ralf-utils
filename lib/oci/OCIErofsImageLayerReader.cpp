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

#include "OCIErofsImageLayerReader.h"
#include "IOCIBackingStore.h"
#include "OCIErofsImageEntry.h"
#include "core/LogMacros.h"
#include "dm-verity/IDmVerityProtectedFile.h"

#include <fcntl.h>
#include <unistd.h>

#include <climits>
#include <cstdarg>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::erofs;
using namespace entos::ralf::dmverity;

#define BLOCK_SIZE uint64_t(4096)

/// The minimum size of a valid EROFS image layer with dm-verity.  We expect both to have a superblock which means
/// at least 4KB for the EROFS image and 4KB for the dm-verity hash tree (even if the image is less than 4KB
#define MIN_EROFSLAYER_SIZE (2 * BLOCK_SIZE)

// -------------------------------------------------------------------------
/*!
    \internal

    Wrapper class to bridge the IDmVerityProtectedFile and IErofsImageFile
    interfaces.  This allows the dm-verity protected file to be used as
    an EROFS image file, so that the EROFS reader code can read the
    contents of the EROFS image layer while also performing dm-verity
    checks on every block read from the image.

 */
class DmVerityErofsImageFile : public IErofsImageFile
{
public:
    explicit DmVerityErofsImageFile(std::shared_ptr<IDmVerityProtectedFile> dmverityFile)
        : m_dmverityFileRef(std::move(dmverityFile))
        , m_dmverityFile(m_dmverityFileRef.get())
        , m_fileSize(m_dmverityFile->size())
    {
    }

    ~DmVerityErofsImageFile() override = default;

    size_t size() const override { return m_fileSize; }

    bool read(void *buffer, size_t size, size_t offset) override { return m_dmverityFile->read(buffer, size, offset); }

    bool advise(Advice advice, size_t size, size_t offset) override
    {
        // Don't support file access advice for dm-verity protected files currently
        return true;
    }

private:
    const std::shared_ptr<IDmVerityProtectedFile> m_dmverityFileRef;
    IDmVerityProtectedFile *const m_dmverityFile;
    const size_t m_fileSize;
};

// -------------------------------------------------------------------------
/*!
    Constructor that takes a mappable file representing the EROFS image layer,
    this includes the file descriptor, offset and size of the image layer.
    The constructor then wraps the file descriptor in a dm-verity reader
    object, so that every block read from the mappable file is checked against
    the dm-verity hash tree.

    The dm-verity protected file is then used by the EROFS reader code to
    read the contents of the EROFS image layer.

 */
OCIErofsImageLayerReader::OCIErofsImageLayerReader(const IOCIBackingStore::MappableFile &imageFile,
                                                   uint64_t hashesOffset, const std::vector<uint8_t> &rootHash)
{
    // Sanity check the offsets and size of the image layer
    if ((imageFile.size < MIN_EROFSLAYER_SIZE) || (imageFile.offset > SIZE_MAX) ||
        ((imageFile.offset + imageFile.size) > SIZE_MAX))
    {
        setError(ErrorCode::PackageContentsInvalid,
                 "EROFS image layer is too small, must be at least %" PRIu64 " bytes", MIN_EROFSLAYER_SIZE);
        return;
    }

    if (hashesOffset > (imageFile.size - BLOCK_SIZE))
    {
        setError(ErrorCode::PackageContentsInvalid,
                 "EROFS image layer hashes offset is invalid (offset:%" PRIu64 ", image size:%" PRIu64 ")",
                 hashesOffset, imageFile.size);
        return;
    }

    const auto dataSize = static_cast<size_t>(hashesOffset);

    const auto hashesSize = static_cast<size_t>(imageFile.size - hashesOffset);
    hashesOffset += imageFile.offset;

    // Create the dm-verity protected file object this will wrap all the reads from the image and perform dm-verity
    // checks on every block read from the image.
    auto result = IDmVerityProtectedFile::open(imageFile.fd, static_cast<size_t>(imageFile.offset), dataSize,
                                               imageFile.fd, static_cast<size_t>(hashesOffset), hashesSize, rootHash);
    if (!result)
    {
        setError(result.error().code(), "Failed to open dm-verity protected file for EROFS image layer: %s",
                 result.error().what());
        return;
    }

    // Wrap the dm-verity protected file in an EROFS image file object, this will allow the EROFS reader code to
    // read the contents of the EROFS image layer while also performing dm-verity checks
    auto erofsImage = std::make_shared<DmVerityErofsImageFile>(std::move(result.value()));

    // Next need to create the EROFS reader object, this will read the contents of the EROFS image layer
    auto erofsReader = IErofsReader::create(std::move(erofsImage));
    if (!erofsReader)
    {
        setError(erofsReader.error().code(), "Failed to create EROFS reader for image layer: %s",
                 erofsReader.error().what());
        return;
    }

    m_erofsReader = std::move(erofsReader.value());
}

OCIErofsImageLayerReader::~OCIErofsImageLayerReader()
{
    m_erofsReader.reset();
}

void OCIErofsImageLayerReader::setError(std::error_code code, const char *format, ...)
{
    std::va_list args;
    va_start(args, format);
    m_error = Error::format(code, format, args);
    va_end(args);
}

std::unique_ptr<IPackageEntryImpl> OCIErofsImageLayerReader::next()
{
    if (!m_erofsReader)
        return nullptr;

    auto entry = m_erofsReader->next();
    if (!entry)
        return nullptr;

    return std::make_unique<OCIErofsImageEntry>(std::move(entry));
}

bool OCIErofsImageLayerReader::hasError() const
{
    return m_error.operator bool() || !m_erofsReader || m_erofsReader->hasError();
}

Error OCIErofsImageLayerReader::error() const
{
    if (m_error)
        return m_error;
    else if (m_erofsReader && m_erofsReader->hasError())
        return m_erofsReader->error();
    else
        return Error(ErrorCode::None);
}
