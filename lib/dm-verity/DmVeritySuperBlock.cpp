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

#include "DmVeritySuperBlock.h"

#include "core/LogMacros.h"

#include <cstring>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::dmverity;

// -----------------------------------------------------------------------------
/*!
    Checks the fields in the supplied super block structure are supported.
    It checks:
        - the signature is valid
        - the version is supported
        - the hash_type is NOT for chromeOS
        - the algorithm is "sha256"
        - the block size(s) are 4096
        - the salt size is valid

    Returns \cc true if the super block is valid / supported by use.

 */
Result<> entos::ralf::dmverity::checkSuperBlock(const VeritySuperBlock *superBlock)
{
    static const char signature[8] = { 'v', 'e', 'r', 'i', 't', 'y', '\0', '\0' };
    if ((memcmp(superBlock->signature, signature, 8) != 0) || (superBlock->version != 1) || (superBlock->hashType != 1))
    {
        return Error(ErrorCode::DmVerityError,
                     "dm-verity superblock doesn't have correct signature or has unsupported hash type");
    }

    // check the algorithm type
    if (memcmp(superBlock->algorithm, "sha256\0", 7) != 0)
    {
        return Error::format(ErrorCode::DmVerityError, "dm-verity superblock has unsupported hash algorithm - '%.32s'",
                             reinterpret_cast<const char *>(superBlock->algorithm));
    }

    //
    if ((superBlock->hashBlockSize != 4096) || (superBlock->dataBlockSize != 4096))
    {
        return Error::format(ErrorCode::DmVerityError,
                             "dm-verity hash or data block size is unsupported (hash: %u, data: %u)",
                             superBlock->hashBlockSize, superBlock->dataBlockSize);
    }

    // sanity check the salt size
    if (superBlock->saltSize > 256)
    {
        return Error(ErrorCode::DmVerityError, "invalid dm-verity salt size");
    }

    return Ok();
}