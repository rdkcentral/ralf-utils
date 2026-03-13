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

#include "DmVerityProtectedFile.h"

#include "core/Compatibility.h"
#include "core/LogMacros.h"

#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Helper function that checks and returns a file range for the given \a fd.
    Both \a offset and \a size are optional, if supplied then they are checked
    to see if they're valid, if not then an empty range is returned.

    If neither \a offset or \a size is supplied then the returned range covers
    the entire file.

 */
static Result<std::pair<size_t, size_t>> checkAndCalcFileRange(int fd, std::optional<size_t> offset,
                                                               std::optional<size_t> size)
{
    // get the size of the file
    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize < 0)
    {
        return Error(std::error_code(errno, std::system_category()), "Failed to seek to end of file");
    }

    // if an offset and / or size was supplied then check they're within the
    // file bounds
    size_t actualOffset = offset.value_or(0);
    if (actualOffset >= static_cast<size_t>(fileSize))
    {
        return Error::format(ErrorCode::DmVerityError, "File offset is outside of the file size (offset %zu, size %zu)",
                             actualOffset, static_cast<size_t>(fileSize));
    }

    size_t actualSize = size.value_or(fileSize - actualOffset);
    if (actualSize > (fileSize - actualOffset))
    {
        return Error::format(ErrorCode::DmVerityError,
                             "Supplied file range is outside of the file size (offset %zu, size %zu, actual size %zu)",
                             actualOffset, actualSize, static_cast<size_t>(fileSize));
    }

    return std::make_pair(actualOffset, actualSize);
}

Result<std::shared_ptr<IDmVerityProtectedFile>>
IDmVerityProtectedFile::open(const std::filesystem::path &dataFile, std::optional<size_t> dataFileOffset,
                             std::optional<size_t> dataFileSize, const std::filesystem::path &hashesFile,
                             std::optional<size_t> hashesFileOffset, std::optional<size_t> hashesFileSize,
                             const std::vector<uint8_t> &rootHash)
{
    int dataFileFd = ::open(dataFile.c_str(), O_CLOEXEC | O_RDONLY);
    if (dataFileFd < 0)
    {
        return Error::format(std::error_code(errno, std::system_category()), "Failed to open data file '%s'",
                             dataFile.c_str());
    }

    int hashesFileFd = ::open(hashesFile.c_str(), O_CLOEXEC | O_RDONLY);
    if (hashesFileFd < 0)
    {
        auto error = Error::format(std::error_code(errno, std::system_category()), "Failed to open hashes file '%s'",
                                   hashesFile.c_str());
        close(dataFileFd);
        return error;
    }

    auto protectedFile = IDmVerityProtectedFile::open(dataFileFd, dataFileOffset, dataFileSize, hashesFileFd,
                                                      hashesFileOffset, hashesFileSize, rootHash);

    // Close the fd's, if the call succeeded then it would have dup'd the fds
    close(dataFileFd);
    close(hashesFileFd);

    return protectedFile;
}

Result<std::shared_ptr<IDmVerityProtectedFile>>
IDmVerityProtectedFile::open(int dataFileFd, std::optional<size_t> dataFileOffset, std::optional<size_t> dataFileSize,
                             int hashesFileFd, std::optional<size_t> hashesFileOffset,
                             std::optional<size_t> hashesFileSize, const std::vector<uint8_t> &rootHash)
{
    if ((dataFileFd < 0) || (hashesFileFd < 0))
        return Error(ErrorCode::InvalidArgument, "Invalid file descriptor(s) supplied");

    // Validate and / or adjust the file offset and size
    auto dataFileRange = checkAndCalcFileRange(dataFileFd, dataFileOffset, dataFileSize);
    if (!dataFileRange)
        return dataFileRange.error();

    auto hashesFileRange = checkAndCalcFileRange(hashesFileFd, hashesFileOffset, hashesFileSize);
    if (!hashesFileRange)
        return hashesFileRange.error();

    // Create the dm-verity verifier object
    auto result =
        IDmVerityVerifier::fromHashesFile(hashesFileFd, rootHash, hashesFileRange->first, hashesFileRange->second);
    if (!result)
        return result.error();

    auto verifier = std::move(result.value());

    // Only support 4k block size right now
    if (verifier->blockSize() != 4096)
        return Error(ErrorCode::DmVerityError, "Only support 4k dm-verity block sizes");

    // Sanity check the dm-very hashes cover the entire data range
    if (dataFileRange->second > (verifier->dataBlockCount() * verifier->blockSize()))
        return Error(ErrorCode::DmVerityError, "dm-verity hashes doesn't cover all the data in the file");

    // dup the data file fd, so we can gift it to created object
    int dupedFd = fcntl(dataFileFd, F_DUPFD_CLOEXEC, 3);
    if (dupedFd < 0)
        return Error(ErrorCode::InvalidArgument, "Failed to dup data file fd");

    // Finally return the protected file object
    return std::make_shared<DmVerityProtectedFile>(std::move(verifier), dupedFd, dataFileRange->first,
                                                   dataFileRange->second);
}

DmVerityProtectedFile::DmVerityProtectedFile(std::shared_ptr<IDmVerityVerifier> verifier, int dataFileFd,
                                             size_t dataOffset, size_t dataSize)
    : m_verifier(std::move(verifier))
    , m_fileFd(dataFileFd)
    , m_fileOffset(dataOffset)
    , m_fileSize(dataSize)
    , m_blockSize(m_verifier->blockSize())
    , m_blockBuffer(reinterpret_cast<uint8_t *>(std::aligned_alloc(4096, m_blockSize)))
{

    // Check if directio is enabled
#if defined(__APPLE__) || defined(EMSCRIPTEN)
    m_directIo = false;
#else
    int flags = fcntl(m_fileFd, F_GETFL, 0);
    if (flags < 0)
    {
        logSysError(errno, "Failed to get fd flags from data file fd");
    }
    else
    {
        m_directIo = ((flags & O_DIRECT) != 0);
        if (m_directIo && ((m_fileOffset % 4096) != 0))
        {
            logWarning("directio enabled on the data file, but the data offset is not block aligned");
        }
    }
#endif
}

DmVerityProtectedFile::~DmVerityProtectedFile()
{
    if (close(m_fileFd) != 0)
        logSysError(errno, "failed to close the file fd");

    std::free(m_blockBuffer);
}

size_t DmVerityProtectedFile::size() const
{
    return m_fileSize;
}

bool DmVerityProtectedFile::read(void *buffer, size_t size, size_t offset)
{
    if ((offset >= m_fileSize) || (size > (m_fileSize - offset)))
    {
        logError("File read outside of bounds (offset=%zu, size=%zu, fileSize=%zu)", offset, size, m_fileSize);
        return false;
    }

    // If in O_DIRECT mode then have to read all blocks via an aligned bounce buffer, hence a different read function
    if (m_directIo)
    {
        return readDirectIo(buffer, size, offset);
    }

    // logDebug("Request to read addr %zu : size %zu", offset, size);

    auto *out = reinterpret_cast<uint8_t *>(buffer);

    // All verifications using dm-verity work on a block basis, so need to read and verify a block at a time
    size_t blkNumber = offset / m_blockSize;

    // If the read is not at the start of the block then need to read and verify a complete block and then copy the
    // bytes we need, that's what readPartialBlock does
    if ((offset % m_blockSize) != 0)
    {
        const size_t blkOffset = offset % m_blockSize;
        const size_t blkAmount = std::min(size, (m_blockSize - blkOffset));

        if (!readPartialBlock(out, blkNumber, blkOffset, blkAmount))
            return false;

        out += blkAmount;
        size -= blkAmount;
        blkNumber++;
    }

    // Read the next full sized blocks
    while (size >= m_blockSize)
    {
        // Read the block
        size_t fileOffset = m_fileOffset + (blkNumber * m_blockSize);
        if (TEMP_FAILURE_RETRY(pread(m_fileFd, out, m_blockSize, fileOffset)) != static_cast<ssize_t>(m_blockSize))
        {
            logSysError(errno, "failed to read block of %zu bytes from data file at offset %zu", m_blockSize, fileOffset);
            return false;
        }

        // Verify it
        if (!m_verifier->verify(blkNumber, out))
        {
            logError("failed to verify block %zu", blkNumber);
            return false;
        }

        logDebug("read and verified block %zu", blkNumber);

        out += m_blockSize;
        size -= m_blockSize;
        blkNumber++;
    }

    // Read any partial tail blocks
    if (size > 0)
    {
        if (!readPartialBlock(out, blkNumber, 0, size))
            return false;
    }

    // Read and verified all blocks
    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads and verifies a block from the file and copies part of the block into the
    \a out buffer.

    The code also stores the verified block and on subsequent calls it will
    re-use the cached buffer if reading from the same block.  This is because the
    typically use case is to linearly read the file, so a partial tail read is
    likely to be followed by a partial head read.

 */
bool DmVerityProtectedFile::readPartialBlock(void *buffer, size_t nblock, size_t offset, size_t size)
{
    // Sanity check the size and offset fit within a block
    if ((offset >= m_blockSize) || (size > (m_blockSize - offset)))
    {
        logError("Partial block read outside of bounds (blockSize=%zu, offset=%zu, size=%zu)", m_blockSize, offset, size);
        return false;
    }

    // Check if read and verified this buffer last time
    if (nblock != m_blockBufferNumber)
    {
        // Otherwise read the block
        size_t fileOffset = m_fileOffset + (nblock * m_blockSize);
        if (TEMP_FAILURE_RETRY(pread(m_fileFd, m_blockBuffer, m_blockSize, fileOffset)) !=
            static_cast<ssize_t>(m_blockSize))
        {
            logSysError(errno, "failed to read %zu bytes from data file at offset %zu", m_blockSize, fileOffset);

            m_blockBufferNumber = SIZE_MAX;
            return false;
        }

        // Verify it
        if (!m_verifier->verify(nblock, m_blockBuffer))
        {
            logError("failed to verify block %zu", nblock);
            m_blockBufferNumber = SIZE_MAX;
            return false;
        }

        // Successfully read and verified the buffer, store the block number for next read
        m_blockBufferNumber = nblock;
    }

    // Copy the data ouf the bounce buffer
    memcpy(buffer, m_blockBuffer + offset, size);
    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads and verifies a single data block, \a buffer must point to a buffer
    of at least block size bytes (at least 4096 bytes).

 */
bool DmVerityProtectedFile::readAndVerifyBlock(size_t nblock, void *buffer) const
{
    // Otherwise read the block
    const size_t fileOffset = m_fileOffset + (nblock * m_blockSize);
    if (TEMP_FAILURE_RETRY(pread(m_fileFd, buffer, m_blockSize, fileOffset)) != static_cast<ssize_t>(m_blockSize))
    {
        logSysError(errno, "failed to read %zu bytes from data file at offset %zu", m_blockSize, fileOffset);
        return false;
    }

    // Verify it
    if (!m_verifier->verify(nblock, buffer))
    {
        logError("failed to verify block %zu", nblock);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Reads blocks from the file and verifies them, then copies the data into the
    supplied output \a buffer.

    This is designed for O_DIRECT files as it uses an aligned block buffer
    for all the reads and then memcpy to the output buffer, rather than writing
    directly to the output buffer.

 */
bool DmVerityProtectedFile::readDirectIo(void *buffer, size_t size, size_t offset)
{
    // logDebug("Request to directio read addr %zu : size %zu", offset, size);

    auto *out = reinterpret_cast<uint8_t *>(buffer);

    // All verifications using dm-verity work on a block basis, so need to read and verify a block at a time
    size_t blkNumber = offset / m_blockSize;

    while (size > 0)
    {
        // Read the block, but check if we've already got it cached
        if (m_blockBufferNumber != blkNumber)
        {
            if (!readAndVerifyBlock(blkNumber, m_blockBuffer))
            {
                // failure already logged
                m_blockBufferNumber = SIZE_MAX;
                return false;
            }

            m_blockBufferNumber = blkNumber;
        }

        // Check if doing a partial read of the block
        const size_t blkOffset = offset % m_blockSize;
        const size_t blkAmount = std::min(size, (m_blockSize - blkOffset));

        // Copy the data into the output buffer
        memcpy(out, m_blockBuffer + blkOffset, blkAmount);

        // On to the next block
        out += blkAmount;
        size -= blkAmount;
        offset = 0;
        blkNumber++;
    }

    // Read and verified all blocks
    return true;
}
