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

#include "DmVerityVerifier.h"
#include "DmVeritySuperBlock.h"

#include "core/Compatibility.h"
#include "core/CryptoDigestBuilder.h"
#include "core/LogMacros.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

// clang-format off
/// Macro for dealing with stuff on block boundaries
#define DIV_ROUND_UP(n, d) (((n) + ((d) - 1)) / (d))
// clang-format on

static constexpr size_t kSha256DigestSize = 32;

using Sha256Digest = std::array<uint8_t, kSha256DigestSize>;

// -----------------------------------------------------------------------------
/*!
    \static
    \internal

    Debug helper to convert a SHA256 digest to a hex string.

 */
static std::string toHex(const Sha256Digest &hash)
{
    static const char hex[] = "0123456789abcdef";
    char str[kSha256DigestSize * 2 + 1];

    for (size_t i = 0; i < kSha256DigestSize; i++)
    {
        str[(i * 2) + 0] = hex[((hash[i] >> 4) & 0xf)];
        str[(i * 2) + 1] = hex[((hash[i] >> 0) & 0xf)];
    }

    str[kSha256DigestSize * 2] = '\0';
    return str;
}

namespace entos::ralf::dmverity
{
    // -----------------------------------------------------------------------------
    /*!
        \interface IHashTreeLayer

        Represents a layer in the dm-verity merkle hash tree.  It has a single API
        to verify a given block from the level below the layer.

     */
    class IHashTreeLayer
    {
    public:
        virtual ~IHashTreeLayer() = default;

        virtual bool verifyBlock(size_t block, const std::array<uint8_t, 32> &digest) const = 0;
    };

    class SingleDataBlockLayer;
    class TopMostHashTreeLayer;
    class HashTreeLayer;

} // namespace entos::ralf::dmverity

class entos::ralf::dmverity::SingleDataBlockLayer : public IHashTreeLayer
{
public:
    explicit SingleDataBlockLayer(const Sha256Digest &rootHash)
        : m_rootHash(rootHash)
    {
    }

    bool verifyBlock(size_t block, const Sha256Digest &digest) const override
    {
        if (block != 0)
            return false;
        else
            return m_rootHash == digest;
    }

private:
    const Sha256Digest m_rootHash;
};

class entos::ralf::dmverity::TopMostHashTreeLayer : public IHashTreeLayer
{
public:
    TopMostHashTreeLayer(int fd, size_t fileOffset, size_t dataBlocks, size_t hashBlockSize,
                         std::vector<uint8_t> salt = {})
        : m_hashesCount(dataBlocks)
        , m_hashesPerBlock(hashBlockSize / kSha256DigestSize)
        , m_hashBlock(nullptr, std::free)
        , m_digest{ 0 }
    {
        // Create an aligned buffer for the hash block in case the file is opened with O_DIRECT
        std::unique_ptr<uint8_t, decltype(std::free) *> hashBlockData(reinterpret_cast<uint8_t *>(
                                                                          std::aligned_alloc(4096, hashBlockSize)),
                                                                      std::free);

        // Read the single hash block
        if (TEMP_FAILURE_RETRY(pread(fd, hashBlockData.get(), hashBlockSize, fileOffset)) !=
            static_cast<ssize_t>(hashBlockSize))
        {
            logSysError(errno, "failed to read hash block from offset %zu", fileOffset);
            return;
        }

        // Calculate and store the salted hash of the block, this must match the root hash
        // and must be checked by higher layers
        m_digest = CryptoDigestBuilder::sha256Digest(hashBlockData.get(), hashBlockSize, salt.data(), salt.size());

        // Store the hash block
        m_hashBlock = std::move(hashBlockData);
    }

    bool verifyBlock(size_t block, const std::array<uint8_t, 32> &digest) const override
    {
        if (block >= m_hashesCount)
        {
            logError("internal error, block %zu is outside of count %zu, should never happen", block, m_hashesCount);
            return false;
        }

        // Calculate the byte offset within the hash block
        const size_t byteOffset = (block % m_hashesPerBlock) * kSha256DigestSize;

        // Compare the hash
        return (memcmp(m_hashBlock.get() + byteOffset, digest.data(), kSha256DigestSize) == 0);
    }

    Sha256Digest digest() const { return m_digest; }

private:
    /// The number of hashes stored in the hash block, this must be less than 4096 / 32
    const size_t m_hashesCount;

    /// The fixed number of hashes that can be stored in the block, this is just the hash block size divided by
    /// SHA256_DIGEST_LENGTH (32)
    const size_t m_hashesPerBlock;

    /// This is the verified top most block of hashes, it has been checked against the root hash to verify it. This
    /// is guaranteed to be hashBlock size bytes in size.
    std::unique_ptr<uint8_t, decltype(std::free) *> m_hashBlock;

    /// The SHA256 hash of the hash block
    Sha256Digest m_digest;
};

class entos::ralf::dmverity::HashTreeLayer : public IHashTreeLayer
{
public:
    HashTreeLayer(std::unique_ptr<IHashTreeLayer> &&parent, int fd, size_t fileOffset, size_t dataBlocks,
                  size_t hashBlockSize, std::vector<uint8_t> salt = {})
        : m_parent(std::move(parent))
        , m_fd(fd)
        , m_fileOffset(fileOffset)
        , m_hashesCount(dataBlocks)
        , m_hashBlockSize(hashBlockSize)
        , m_hashesPerBlock(hashBlockSize / kSha256DigestSize)
        , m_salt(std::move(salt))
    {
    }

    bool verifyBlock(size_t block, const Sha256Digest &digest) const override
    {
        logDebug("Verifying block %zu against sha256: %s", block, toHex(digest).c_str());

        if (block >= m_hashesCount)
        {
            logError("Internal error, supplied block %zu is larger than hashes count %zu", block, m_hashesCount);
            return false;
        }

        // Get the hash block that will contain the hash for the data block
        const size_t hashBlockIndex = block / m_hashesPerBlock;
        logDebug("Verifying block index %zu", hashBlockIndex);

        auto it = m_verifiedHashBlocks.find(hashBlockIndex);
        if (it == m_verifiedHashBlocks.end())
        {
            const size_t fileOffset = m_fileOffset + (hashBlockIndex * m_hashBlockSize);

            // Create an aligned buffer for the hash block in case the file is opened with O_DIRECT
            std::unique_ptr<uint8_t, decltype(std::free) *> hashBlockData(reinterpret_cast<uint8_t *>(
                                                                              std::aligned_alloc(4096, m_hashBlockSize)),
                                                                          std::free);

            // Read the hash block from the file
            if (TEMP_FAILURE_RETRY(pread(m_fd, hashBlockData.get(), m_hashBlockSize, fileOffset)) !=
                static_cast<ssize_t>(m_hashBlockSize))
            {
                logSysError(errno, "Failed to read hash block at offset %zu", fileOffset);
                return false;
            }

            // calculate the SHA256 of the hash block just read
            const Sha256Digest hash =
                CryptoDigestBuilder::sha256Digest(hashBlockData.get(), m_hashBlockSize, m_salt.data(), m_salt.size());

            logDebug("sha256 of hash block is %s", toHex(hash).c_str());

            // Require a parent; we cache the very top most layers hash block after comparing to the root hash, so
            // should never hit this
            if (!m_parent || !m_parent->verifyBlock(hashBlockIndex, hash))
            {
                return false;
            }

            // If cache is full, remove an item from the cache
            if (m_verifiedHashBlocks.size() >= kMaxCacheBlocks)
            {
                // The assumption is that this hash tree is used to protect a EROFS image, and we generally read
                // through that sequentially from file  start to end to extract everything ... so remove the hash block
                // at the beginning of the cache as that is least likely to be used again
                m_verifiedHashBlocks.erase(m_verifiedHashBlocks.begin());
            }

            // Add to the cache
            it = m_verifiedHashBlocks.emplace(hashBlockIndex, std::move(hashBlockData)).first;
        }

        // Verified the hash block against the root hash so now just check the supplied digest matches what was in the
        // hash block read from the file

        // Calculate the byte offset within the hash block
        const size_t byteOffset = (block % m_hashesPerBlock) * kSha256DigestSize;
        // logDebug("VerifyBlock : byteOffset=%zu", byteOffset);

        // Compare the hash
        return (memcmp(it->second.get() + byteOffset, digest.data(), kSha256DigestSize) == 0);
    }

private:
    /// The pointer to the layer 'above' us in the tree
    const std::unique_ptr<IHashTreeLayer> m_parent;

    /// The file storing the hashes
    const int m_fd;

    /// The offset within the file of the first hash block in the layer
    const size_t m_fileOffset;

    /// The total number of hashes stored in the layer
    const size_t m_hashesCount;

    /// The size of a hash block, currently fixed at 4096 bytes
    const size_t m_hashBlockSize;

    /// The fixed number of hashes that can be stored in the block, this is just the
    /// hash block size divided by SHA256_DIGEST_LENGTH (32)
    const size_t m_hashesPerBlock;

    /// The salt used in the SHA256 hash calculations, may be empty (in which case not used)
    std::vector<uint8_t> m_salt;

    /// The maximum number of hash blocks to cache, at 4K hash blocks this gives a max
    /// memory usage of 256KB
    static constexpr size_t kMaxCacheBlocks = 64;

    /// Cache of verified hash blocks
    mutable std::map<size_t, std::unique_ptr<uint8_t, decltype(std::free) *>> m_verifiedHashBlocks;
};

Result<std::shared_ptr<IDmVerityVerifier>> IDmVerityVerifier::fromHashesFile(const std::filesystem::path &hashesFile,
                                                                             const std::vector<uint8_t> &rootHash,
                                                                             std::optional<size_t> hashesOffset,
                                                                             std::optional<size_t> hashesSize)
{
    int fd = open(hashesFile.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        return Error(std::error_code(errno, std::system_category()));
    }

    // Create the verifier object (it takes ownership of the fd)
    auto dmVerity = std::make_shared<DmVerityVerifier>();
    auto result = dmVerity->open(fd, rootHash, hashesOffset, hashesSize);
    if (result.isError())
    {
        close(fd);
        return result.error();
    }

    return dmVerity;
}

Result<std::shared_ptr<IDmVerityVerifier>> IDmVerityVerifier::fromHashesFile(int hashesFileFd,
                                                                             const std::vector<uint8_t> &rootHash,
                                                                             std::optional<size_t> hashesOffset,
                                                                             std::optional<size_t> hashesSize)
{
    // dup the fd so the verifier has its own copy
    int fd = fcntl(hashesFileFd, F_DUPFD_CLOEXEC, 3);
    if (fd < 0)
    {
        return Error(std::error_code(errno, std::system_category()));
    }

    // Create the verifier object (it takes ownership of the fd)
    auto dmVerity = std::make_shared<DmVerityVerifier>();
    auto result = dmVerity->open(fd, rootHash, hashesOffset, hashesSize);
    if (result.isError())
    {
        close(fd);
        return result.error();
    }

    return dmVerity;
}

Result<> DmVerityVerifier::open(int hashesFileFd, const std::vector<uint8_t> &rootHash,
                                std::optional<size_t> hashesOffset, std::optional<size_t> hashesSize)
{
    // Sanity check the root hash is valid
    if (rootHash.size() != kSha256DigestSize)
        return Error::format(ErrorCode::DmVerityError, "Invalid root hash size (%zu)", rootHash.size());

    memcpy(m_rootHash.data(), rootHash.data(), kSha256DigestSize);

    // Get the size of the source file
    off_t fileSize = lseek(hashesFileFd, 0, SEEK_END);
    if (fileSize < 0)
        return Error(std::error_code(errno, std::system_category()), "Failed to get size of hashes file");

    // If an offset and / or size was supplied then check they're within the file bounds
    m_hashesOffset = hashesOffset.value_or(0);
    if (m_hashesOffset > static_cast<size_t>(fileSize))
    {
        return Error::format(ErrorCode::DmVerityError, "Hashes offset is outside of the file size (offset %zu, size %zu)",
                             m_hashesOffset, static_cast<size_t>(fileSize));
    }

    m_hashesSize = hashesSize.value_or(fileSize - m_hashesOffset);
    if (m_hashesSize > (fileSize - m_hashesOffset))
    {
        return Error::format(ErrorCode::DmVerityError, "Supplied hashes range is outside of the file size (offset %zu, size %zu, actual size %zu)",
                             m_hashesOffset, m_hashesSize, static_cast<size_t>(fileSize));
    }

    // Checks the hashes offset and size and valid for the supplied fd and that the superblock is correct for the fd
    // ... before finally checking the root hash against the root hash stored in the file
    auto result = parseDmVeritySuperBlock(hashesFileFd);
    if (result.isError())
    {
        return result.error();
    }

    // Everything checks out so take ownership of the file descriptor
    m_hashesFd = hashesFileFd;
    return Ok();
}

DmVerityVerifier::~DmVerityVerifier()
{
    if ((m_hashesFd >= 0) && (close(m_hashesFd) != 0))
        logSysError(errno, "Failed to close hashes fd");
}

size_t DmVerityVerifier::blockSize() const
{
    return m_dataBlockSize;
}

size_t DmVerityVerifier::dataBlockCount() const
{
    return m_dataBlockCount;
}

Result<> DmVerityVerifier::parseDmVeritySuperBlock(int fd)
{
    static_assert(sizeof(VeritySuperBlock) < 4096, "invalid superblock size");

    // In case of directio file create an aligned buffer to read the superblock
    auto *superBlockBuf = reinterpret_cast<uint8_t *>(std::aligned_alloc(4096, 4096));
    if (TEMP_FAILURE_RETRY(pread(fd, superBlockBuf, 4096, m_hashesOffset)) != static_cast<ssize_t>(4096))
    {
        Error error(std::error_code(errno, std::system_category()), "Failed to read superblock");
        std::free(superBlockBuf);
        return error;
    }

    VeritySuperBlock superBlock = {};
    memcpy(&superBlock, superBlockBuf, sizeof(VeritySuperBlock));
    std::free(superBlockBuf);

    // Sanity check the super block is supported
    const auto result = checkSuperBlock(&superBlock);
    if (result.isError())
        return result.error();

    m_hashBlockSize = superBlock.hashBlockSize;
    m_dataBlockSize = superBlock.dataBlockSize;
    m_dataBlockCount = superBlock.dataBlocks;

    // Limit the data size to 512MB for now (128K blocks * 4096 block size = 512 MB)
    if ((m_dataBlockCount == 0) || ((m_dataBlockCount * m_dataBlockSize) > (512 * 1024 * 1024)))
    {
        return Error::format(ErrorCode::DmVerityError, "dm-verity has too large data block count (%zu)",
                             m_dataBlockCount);
    }

    // Read and store the salt
    if (superBlock.saltSize > 0)
    {
        m_salt.resize(superBlock.saltSize);
        memcpy(m_salt.data(), superBlock.salt, superBlock.saltSize);
    }

    // Calculate the number of hashes store in a single hash block
    m_hashesPerBlock = (m_hashBlockSize / kSha256DigestSize);

    // Special case if there is only one data block, in that case there is just the root hash which covers the single
    // data block
    if (m_dataBlockCount == 1)
    {
        m_layer0 = std::make_unique<SingleDataBlockLayer>(m_rootHash);
        return Ok();
    }

    // Otherwise work out the number of hash blocks and data blocks covered in each layer of the tree, starting at the
    // bottom most layer
    struct LayerDetails
    {
        size_t hashBlocks; // number of hash blocks required for the data blocks
        size_t dataBlocks; // number of data blocks covered by this layer
    };

    std::vector<LayerDetails> layerDetails;
    layerDetails.reserve(16);

    // Get the number of hash blocks needed to cover the data
    size_t levelHashBlocks = DIV_ROUND_UP(m_dataBlockCount, m_hashesPerBlock);
    layerDetails.push_back({ levelHashBlocks, m_dataBlockCount });

    size_t totalHashBlocks = levelHashBlocks;

    // The other levels
    while (levelHashBlocks > 1)
    {
        // the number of data blocks is the number of hash blocks from the previous layer
        size_t dataBlocks = levelHashBlocks;

        // calculate and store the number of data and hash blocks in this layer
        levelHashBlocks = DIV_ROUND_UP(dataBlocks, m_hashesPerBlock);
        layerDetails.push_back({ levelHashBlocks, dataBlocks });

        totalHashBlocks += levelHashBlocks;
    }

    // Reverse the vector, the entry at index 0 is now the layer at the top of the tree
    std::reverse(layerDetails.begin(), layerDetails.end());

    // Check that all the required hash blocks fit within the file, there is an extra hash block at the start of the
    // file to store the superblock
    if (((1 + totalHashBlocks) * m_hashBlockSize) > m_hashesSize)
    {
        return Error::format(ErrorCode::DmVerityError,
                             "Number of required hash blocks (%zu) of size %zu exceeds hashes file size (%zu)",
                             totalHashBlocks, m_hashBlockSize, m_hashesSize);
    }

    // The top most level is located one hash block size after the superblock
    size_t layerOffset = m_hashesOffset + m_hashBlockSize;

    // Iterate through the layers, top down
    std::unique_ptr<IHashTreeLayer> layer =
        std::make_unique<TopMostHashTreeLayer>(fd, layerOffset, layerDetails[0].dataBlocks, m_hashBlockSize, m_salt);

    // The hash of the top layer in the tree MUST match the root hash
    if (dynamic_cast<TopMostHashTreeLayer *>(layer.get())->digest() != m_rootHash)
    {
        logError("dm-verity root hash doesn't match\n"
                 "\texpected: %s\n"
                 "\tactual:   %s",
                 toHex(m_rootHash).c_str(), toHex(dynamic_cast<TopMostHashTreeLayer *>(layer.get())->digest()).c_str());
        return Error::format(ErrorCode::DmVerityError, "dm-verity root hash doesn't match");
    }

    // Go through to the next layers down the tree (if any)
    for (size_t i = 1; i < layerDetails.size(); i++)
    {
        // move to the next layer offset
        layerOffset += (layerDetails[i - 1].hashBlocks * m_hashBlockSize);

        logDebug("layer %zu => offset 0x%06zx : hash blocks %zu : data blocks %zu", i, layerOffset,
                 layerDetails[i].hashBlocks, layerDetails[i].dataBlocks);

        // Create a new layer
        layer = std::make_unique<HashTreeLayer>(std::move(layer), fd, layerOffset, layerDetails[i].dataBlocks,
                                                m_hashBlockSize, m_salt);
    }

    // Store the last layer as the one we start the verification with
    m_layer0 = std::move(layer);

    return Ok();
}

bool DmVerityVerifier::verify(size_t block, const void *data) const
{
    if (!m_layer0)
        return false;

    if (block >= m_dataBlockCount)
        return false;

    // Calculate the SHA256 of the data
    const Sha256Digest digest = CryptoDigestBuilder::sha256Digest(data, m_dataBlockSize, m_salt.data(), m_salt.size());
    // logDebug("data block %zu sha256 : %s", block, toHex(digest).c_str());

    // Check the hash with the tree
    return m_layer0->verifyBlock(block, digest);
}
