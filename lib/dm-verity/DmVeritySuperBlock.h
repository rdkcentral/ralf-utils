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

#include "LibRalf.h"
#include "Result.h"

#include <cinttypes>

/// The superblock format
struct VeritySuperBlock
{
    uint8_t signature[8];   // "verity\0\0"
    uint32_t version;       // superblock version
    uint32_t hashType;      // 0 - Chrome OS, 1 - normal
    uint8_t uuid[16];       // UUID of hash device
    uint8_t algorithm[32];  // hash algorithm name
    uint32_t dataBlockSize; // data block in bytes
    uint32_t hashBlockSize; // hash block in bytes
    uint64_t dataBlocks;    // number of data blocks
    uint16_t saltSize;      // salt size
    uint8_t pad1[6];
    uint8_t salt[256]; // salt
    uint8_t pad2[168];
} __attribute__((packed));

static_assert(sizeof(VeritySuperBlock) == 512, "Invalid packing of VeritySuperBlock");

namespace entos::ralf::dmverity
{

    // -----------------------------------------------------------------------------
    /*!
        \internal

        Checks the fields in the supplied super block structure are supported.
        It checks:
            - the signature is valid
            - the version is supported
            - the hash_type is NOT for chromeOS
            - the algorithm is "sha256"
            - the block size(s) are 4096
            - the salt size is valid

        Returns a valid result if the super block is valid, otherwise an error.

     */
    LIBRALF_NS::Result<> checkSuperBlock(const VeritySuperBlock *superBlock);

} // namespace entos::ralf::dmverity