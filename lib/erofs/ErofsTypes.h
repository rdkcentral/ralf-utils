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

#include <cinttypes>

// -----------------------------------------------------------------------------
/*!
    This file contains the EROFS structures from the kernel headers.

    \see https://erofs.docs.kernel.org/en/latest/core_ondisk.html
    \see https://elixir.bootlin.com/linux/latest/source/fs/erofs/erofs_fs.h
 */

/// The ERFOS super block is at 1K offset from the start of the image, this is so you can add other stuff at the
/// beginning.  We currently don't utilise this space but it is there if we need it.
/// \see https://erofs.docs.kernel.org/en/latest/core_ondisk.html
#define EROFS_SUPER_OFFSET 1024

/// Magic first 4 bytes of super block, this is always at offset 1024 in the image and is little-endian
#define EROFS_MAGIC 0xE0F5E1E2

struct ErofsSuperBlockStruct
{
    uint32_t magic;         ///< file system magic number
    uint32_t checksum;      ///< crc32c of the first block starting at superblock offset
    uint32_t featureCompat; ///< Compatible feature flags. We can still read the image even
                            ///  if don't understand the flag
    uint8_t blockSizeBits;  ///< filesystem block size in bit shift, ie. 12 is 4096 bytes
    uint8_t extensionSlots; ///< superblock size = 128 + sb_extslots * 16

    uint16_t rootInode;  ///< the inode number of the root directory
    uint64_t inodeCount; ///< total valid inode count

    uint64_t buildTimeSec;    ///< the time the image was created in seconds since epoch
    uint32_t buildTimeNSec;   ///< the nanosecond component of the build time
    uint32_t blocks;          ///< total block count
    uint32_t metaBlockAddr;   /// start block number of metadata area, the superblock is in block 0
    uint32_t xattrBlockAddr;  /// start block number of shared xattr area
    uint8_t volumeUuid[16];   ///< 128-bit uuid for volume
    uint8_t volumeName[16];   ///< volume name
    uint32_t featureIncompat; ///< Incompatible feature flags. We'll fail to
                              /// read image if we don't accept a flag.
    union
    {
        uint16_t availableComprAlgs; ///<  bitmap for available compression algorithms (FEATURE_INCOMPAT_COMPR_CFGS is set)
        uint16_t lz4MaxDistance; ///< customized sliding window size instead of 64k by default (FEATURE_INCOMPAT_COMPR_CFGS not set)
    } __attribute__((packed)) u1;

    uint16_t extraDevices;       ///< # of devices besides the primary device
    uint16_t devtSlotOff;        ///< startoff = devt_slotoff * devt_slotsize
    uint8_t dirBlkBits;          ///< unused
    uint8_t xattrPrefixCount;    ///< # of long xattr name prefixes */
    uint32_t xattrPrefixStart;   ///< start of long xattr prefixes */
    uint64_t packedInodeNumber;  ///< Inode number of the special packed inode
    uint8_t xattrFilterReserved; ///< reserved for xattr name filter

    uint8_t reserved[23];
} __attribute__((packed));
static_assert(sizeof(ErofsSuperBlockStruct) == 128, "Invalid Erofs superblock struct size packing");

/// Feature bits that don't break compatibility
enum ErofsCompatFeature : uint32_t
{
    SuperBlockChecksum = (1 << 0),
    ModificationTime = (1 << 1),
    XattrFilter = (1 << 2),

};

/// Feature bits in the [featureIncompat] field, we only support "zero padding" feature, if any other feature flag is
/// present we cannot extract the image.
enum ErofsIncompatFeature : uint32_t
{
    ZeroPadding = (1 << 0),
    ComprCfgs = (1 << 1),
    BigPcluster = (1 << 1),
    ChunkedFile = (1 << 2),
    DeviceTable = (1 << 3),
    ComprHead2 = (1 << 3),
    Ztailpacking = (1 << 4),
    Fragments = (1 << 5),
    Dedupe = (1 << 5),
    XattrPrefixes = (1 << 6),

};

enum class ErofsInodeFormat : uint16_t
{
    FlatPlain = 0,
    CompressedFull = 1,
    FlatInline = 2,
    CompressedCompact = 3,
    ChunkBased = 4,
};

union ErofsInodeData
{
    uint32_t compressedBlocks; ///< Total compressed blocks for compressed inodes

    uint32_t rawBlkAddr; ///< Block address for uncompressed flat inodes

    uint32_t rdev; ///< For device files, used to indicate old/new device #

    /// There is another structure here for chunk-based files but we don't support those
};
static_assert(sizeof(ErofsInodeData) == 4, "Invalid Erofs inode data details");

/// Represents a 32-byte 'on-disk' EROFS inode structure.
struct ErofsInodeCompact
{
    uint16_t format; ///< format hints, if bit 0 is 0 then compact inode (32-byte), if 1 then extended inode (64-byte)

    uint16_t xattrCount;
    uint16_t mode;
    uint16_t nlink;
    uint32_t size;
    uint32_t reserved;
    union ErofsInodeData data;

    uint32_t ino;
    uint16_t uid;
    uint16_t gid;
    uint32_t reserved2;
} __attribute__((packed));
static_assert(sizeof(ErofsInodeCompact) == 32, "Invalid Erofs compat inode size");

/// Represents a 64-byte 'on-disk' EROFS inode structure.
struct ErofsInodeExtended
{
    uint16_t format; ///< format hints

    uint16_t xattrCount;
    uint16_t mode;
    uint16_t reserved;
    uint64_t size;
    union ErofsInodeData data;

    uint32_t ino;
    uint32_t uid;
    uint32_t gid;
    uint64_t mtimeSecs;
    uint32_t mtimeNSecs;
    uint32_t nlink;
    uint8_t reserved2[16];
} __attribute__((packed));
static_assert(sizeof(ErofsInodeExtended) == 64, "Invalid Erofs extended inode size");

/// File types used in ErofsDirEntry::fileType field, these are not the same as the POSIX file types used in stat()
enum class ErofsFileType : uint8_t
{
    Unknown = 0,     ///< Unknown file type, should not be used
    RegularFile = 1, ///< Regular file
    Directory = 2,   ///< Directory
    CharDevice = 3,  ///< Character device
    BlockDevice = 4, ///< Block device
    Fifo = 5,        ///< FIFO / named pipe
    Socket = 6,      ///< Socket
    Symlink = 7,     ///< Symbolic link

    Max = 8 ///< Not a valid file type, just used to indicate the max value
};

struct ErofsDirEntry
{
    uint64_t nid;        ///< EROFS inode number
    uint16_t nameOffset; ///< start offset of file name
    uint8_t fileType;    ///< file type, see above ErofsFileType::* enums
    uint8_t reserved;
} __attribute__((packed));
static_assert(sizeof(ErofsDirEntry) == 12, "Invalid Erofs dirent size");

/// The maximum allowed length of a filename in EROFS
#define EROFS_MAX_NAME_LEN 255

/// Available compression algorithm types (for ErofsZMapHeader::algorithmType) */
enum class ErofsCompressionAlgorithm : uint8_t
{
    Lz4 = 0,
    Lzma = 1,
    Deflate = 2,
    Zstd = 3,

    Max = 4,
};

/// Possible bits in the ErofsZMapHeader::advise field, we only support the 'Compacted2b' bit, if any other bit is
/// set then we cannot extract the image.
enum ErofsZMapHeaderAdviseBits : uint16_t
{
    Compacted2b = 0x0001,
    Head1BigPcluster = 0x0002,
    Head2BigPcluster = 0x0004,
    TailPackingInlinePcluster = 0x0008,
    InterlacedPlainPcluster = 0x0010,
    FragmentPcluster = 0x0020,
};

/// Data structure for the compressed inode map header
struct ErofsZMapHeader
{
    union
    {
        uint32_t fragmentOff; ///< Offset of fragment data in the packed inode
        struct
        {
            uint16_t reserved1;
            uint16_t dataSize; ///< Indicates the encoded size of tailpacking data
        };
    };

    uint16_t advise;       ///< Various flags advising how the logical clusters are stored
    uint8_t algorithmType; ///< Compression algorithm types for head1 (bits 0-3) and head2 (bits 4-7)
    uint8_t clusterBits;   ///< bit 0-2 : logical cluster bits - 12, e.g. 0 for 4096;
                           ///  bit 3-6 : reserved;
                           ///  bit 7   : move the whole file into packed inode or not.
} __attribute__((packed));
static_assert(sizeof(ErofsZMapHeader) == 8, "Invalid z_erofs_map_header struct packing");

/// The type of logical cluster entry
enum class ErofsLclusterType : uint8_t
{
    Plain = 0,
    Head1 = 1,
    NonHead = 2,
    Head2 = 3,

    Max
};

/// Data structure for each logical cluster index entry
struct ErofsLclusterIndex
{
    uint16_t advise; ///< Bits 0:2 are the logical cluster type, see ErofsLclusterType above.

    uint16_t clusterOffset; ///< Where to decompress the cluster to in the uncompressed data, this is an offset
                            /// within the logical cluster so must be less than the logical cluster size

    union
    {
        uint32_t blkAddr; ///< For head entries, this is the block address of the (compressed or uncompressed) physical cluster
        uint16_t delta[2]; ///< For non-head entries, these are the distances to the previous and next head entries
    } data;

} __attribute__((packed));
static_assert(sizeof(ErofsLclusterIndex) == 8, "Invalid ErofsLclusterIndex struct packing");
