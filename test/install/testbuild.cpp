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

#include <ralf/Certificate.h>
#include <ralf/LibRalf.h>
#include <ralf/Logging.h>
#include <ralf/Package.h>
#include <ralf/PackageEntry.h>
#include <ralf/PackageMetaData.h>
#include <ralf/PackageMount.h>
#include <ralf/PackageReader.h>
#include <ralf/VerificationBundle.h>
#include <ralf/VersionConstraint.h>
#include <ralf/VersionNumber.h>

#include <iostream>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

int main(int argc, char *argv[])
{
    std::cout << "LibRalf Version: " << libRalfVersionMajor() << "." << libRalfVersionMinor() << "."
              << libRalfVersionMicro() << " (" << libRalfVersionString() << ")" << std::endl;

    setLogHandler(stdoutLogHandler);
    setLogLevel(nullptr, LogPriority::Info);

    auto cert = Certificate::loadFromFile("test.crt", Certificate::EncodingFormat::PEM);

    VerificationBundle bundle;
    bundle.addCertificate(cert.value());

    auto package = Package::open("test.wgt", bundle);
    package->verify();
    package->extractTo("output");

    auto metadata = package->metaData();

    return 0;
}
