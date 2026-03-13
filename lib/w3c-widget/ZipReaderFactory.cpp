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

#include "ZipReaderFactory.h"
#include "archive/LibarchiveFileReader.h"

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

using namespace entos::ralf::archive;

// -------------------------------------------------------------------------
/*!
    Creates a libarchive reader object for a zip file and attaches it to
    package file descriptor stored in this object.  If \a autoCloseFd is
    \c true then the file descriptor will be closed when the archive object
    is destructed.

    It replaces the standard archive reader functions with ones that use
    pread instead of read, this allows us to have multiple concurrent readers
    on the same file descriptor as the file offset is not changing.

    On failure an Error result is returned, otherwise a unique_ptr to the
    archive reader object.

    \warning The \a fd is not dup'ed and stored in the returned archive object,
    therefore it's important the \a fd is not closed while the archive object
    exists.

*/
Result<std::unique_ptr<ILibarchiveReader>> ZipReaderFactory::createReader(int archiveFd, size_t archiveSize,
                                                                          bool autoCloseFd)
{
    Error error;

    auto archive =
        std::make_unique<LibarchiveFileReader>(archiveFd, archiveSize, ArchiveFormat::Zip, autoCloseFd, &error);
    if (!archive || archive->isNull())
        return error;
    else
        return archive;
}
