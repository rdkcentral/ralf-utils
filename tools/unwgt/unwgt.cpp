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

#include "Logging.h"
#include "Package.h"
#include "PackageReader.h"
#include "VerificationBundle.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <list>
#include <string>

#include <fcntl.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>

#if defined(LIBRALF_NS)
using namespace LIBRALF_NS;
#endif

/// The operation to perform, either extract or mount
static enum {
    NotSpecified,
    VerifyPackage,
    ListContents,
    ExtractPackage,
    MountPackage,
    DumpMetadata,
    DumpAuxMetadata,
    DumpSignatureInfo,
} gOperation = NotSpecified;

/// Path to the source package, this is mandatory
static std::filesystem::path gPackageFilePath;

/// The path to extract into, defaults to current working path
static std::filesystem::path gOutputDirectory = std::filesystem::current_path();

/// If --mount option was supplied this sets the mount point path
static std::filesystem::path gMountPoint;

/// The path to a directory to read all the certificates from
static std::filesystem::path gCertificatesDir = "/etc/sky/certs/";

/// The certificates to use for verification
static std::list<std::filesystem::path> gCertificates;

/// The file(s) to extract from the package, if empty then all files are extracted
static std::set<std::filesystem::path> gFilesFilter;

/// If \c true then no verification will take place
static bool gVerifyPackage = true;

/// If \c true the verification will fail if the certificates used for
/// signing / verification have expired.  By default we ignore certificate expiry
static bool gEnableCertificateTimeCheck = false;

/// \c true if --verbose was passed as an argument, used for verbose --list output
static LogPriority gLogLevel = LogPriority::Warning;

/// The format to dump the package metadata in, defaults to raw format which means just dump the raw metadata file
/// from the package.
static enum class MetaDataDumpFormat {
    Raw,
    Json,
    Xml,
} gDumpMetaDataFormat = MetaDataDumpFormat::Raw;

/// Non-public option to force the generation of the package metadata even if the package has the metadata in a format
/// requested by the user.  This is useful to test the metadata parsing code, by forcing the (re)generation of the
/// metadata from the parsed data.
static bool gForceMetadataGeneration = false;

/// The type of aux metadata to dump / extract
static std::optional<std::string> gAuxMetaDataMediaType;

// -----------------------------------------------------------------------------
/*!
    Helper function in configxml.cpp that converts the package meta-data to a
    config.xml format.  This is used to dump the package metadata in XML format
    when the -M option is used.
*/
extern std::string metaDataToConfigXml(const PackageMetaData &metaData);

// -----------------------------------------------------------------------------
/*!
    Helper function in configspec.cpp that converts the package meta-data to a
    OCI artifact JSON spec format.  This is used to dump the package metadata
    in JSON format when the -M option is used.
*/
extern std::string metaDataToConfigSpec(const PackageMetaData &metaData);

// -----------------------------------------------------------------------------
/*!
    Simply prints the version string on stdout.

 */
static void displayVersion()
{
    printf("Version: 1.0\n");
}

// -----------------------------------------------------------------------------
/*!
    Simply prints the usage options to stdout.

 */
static void displayUsage()
{
    printf("Usage: unwgt -x -f PACKAGE [OPTIONS] [MEMBER...]\n"
           "Usage: unwgt -t -f PACKAGE [OPTIONS] [MEMBER...]\n"
           "Usage: unwgt -m [MOUNTPOINT] -f PACKAGE [OPTIONS]\n"
           "  Extracts or mounts widget / package file optionally with verification.\n");
    printf("\n");
    printf("  -h, --help                     Print this help and exit.\n");
    printf("  -v, --verbose                  Increase the log level, can be specified multiple times.\n");
    printf("  -V, --version                  Display this program's version number.\n");
    printf("\n");
    printf("      --verify                   Performs a full verification on the package returning the result.\n"
           "                                 Extra arguments are ignored.\n");
    printf("  -t, --list                     List the contents of an archive.  Arguments are optional.\n"
           "                                 When given, they specify the names of the members to list.\n");
    printf("  -x, --extract                  Extract files from a widget.  Arguments are optional.\n"
           "                                 When given, they specify names of the archive members to\n"
           "                                 be extracted.\n");
#if defined(__linux__)
    printf("  -m, --mount=<MOUNTPOINT>       Mount the package rather than extract (only for EROFS packages).\n");
#endif
    printf("  -M  --metadata=<FORMAT>        Prints the package meta data in the given FORMAT.\n"
           "                                 FORMAT is optional, if specified it can be \"raw\" (default), \"json\" or "
           "\"xml\".\n");
    printf("  -X  --aux-metadata=<TYPE>      If no arguments is supplied then will display the media types of all aux\n"
           "                                 metadata files in the package.  If TYPE is given then will extract the\n"
           "                                 aux metadata file with the given media TYPE.\n");
    printf("  -S  --siginfo                  Display information on the package signature.\n");
    printf("\n");
    printf("  -f, --file=<WIDGET>            The widget file to extract or mount.\n");
    printf("  -C, --directory=<DIRECTORY>    Extract to the given directory rather than the current directory.\n");
    printf("\n");
    printf("  -n, --no-verify                Skip verification of the package.\n");
    printf("      --cert-dir=<DIRECTORY>     The directory storing the verification certificates [%s]\n",
           gCertificatesDir.c_str());
    printf("  -c, --cert=<CERTIFICATE>       Path to verifier certificate, can be specified multiple times.\n");
    printf("                                 If set then cert-dir is not used.\n");
    printf("  -e, --enable-cert-expiry-check Enable checking of the certificate expiry time when verifying.\n"
           "                                 By default expiry time is not checked.\n");
    printf("\n");
}

// -----------------------------------------------------------------------------
/*!
    Parses the command line args

 */
static void parseArgs(int argc, char **argv)
{
    struct option longopts[] = { { "help", no_argument, nullptr, (int)'h' },
                                 { "verbose", no_argument, nullptr, (int)'v' },
                                 { "version", no_argument, nullptr, (int)'V' },
                                 { "file", required_argument, nullptr, (int)'f' },
                                 { "directory", required_argument, nullptr, (int)'C' },
                                 { "list", no_argument, nullptr, (int)'t' },
                                 { "verify", no_argument, nullptr, (int)'y' },
                                 { "extract", no_argument, nullptr, (int)'x' },
                                 { "mount", required_argument, nullptr, (int)'m' },
                                 { "metadata", optional_argument, nullptr, (int)'M' },
                                 { "aux-metadata", optional_argument, nullptr, (int)'X' },
                                 { "no-verify", no_argument, nullptr, (int)'n' },
                                 { "cert-dir", required_argument, nullptr, (int)'d' },
                                 { "cert", required_argument, nullptr, (int)'c' },
                                 { "enable-cert-expiry-check", no_argument, nullptr, (int)'e' },
                                 { "siginfo", no_argument, nullptr, (int)'S' },
                                 { "force-metadata-gen", no_argument, nullptr, (int)'z' },
                                 { nullptr, 0, nullptr, 0 } };

    int c;
    int longindex;
    while ((c = getopt_long(argc, argv, "hvVxtM::f:C:m:nc:eSz", longopts, &longindex)) != -1)
    {
        switch (c)
        {
            case 'h':
                displayUsage();
                exit(EXIT_SUCCESS);

            case 'v':
                if (gLogLevel == LogPriority::Warning)
                    gLogLevel = LogPriority::Info;
                else if (gLogLevel == LogPriority::Info)
                    gLogLevel = LogPriority::Debug;
                break;

            case 'V':
                displayVersion();
                exit(EXIT_SUCCESS);

            case 'f':
                gPackageFilePath = optarg;
                break;

            case 'C':
                gOutputDirectory = optarg;
                if (!std::filesystem::is_directory(gOutputDirectory))
                {
                    fprintf(stderr, "Error: output directory '%s' doesn't exist or is not a directory\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            case 't':
                gOperation = ListContents;
                break;

            case 'x':
                gOperation = ExtractPackage;
                break;

            case 'y':
                gOperation = VerifyPackage;
                break;

            case 'M':
                gOperation = DumpMetadata;
                if (optarg)
                {
                    if (strcasecmp(optarg, "raw") == 0)
                        gDumpMetaDataFormat = MetaDataDumpFormat::Raw;
                    else if (strcasecmp(optarg, "json") == 0)
                        gDumpMetaDataFormat = MetaDataDumpFormat::Json;
                    else if (strcasecmp(optarg, "xml") == 0)
                        gDumpMetaDataFormat = MetaDataDumpFormat::Xml;
                    else
                    {
                        fprintf(stderr, "Error: unknown metadata format '%s'\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'X':
                gOperation = DumpAuxMetadata;
                if (optarg)
                {
                    gAuxMetaDataMediaType = optarg;
                }
                break;

            case 'z':
                gForceMetadataGeneration = true;
                break;

            case 'S':
                gOperation = DumpSignatureInfo;
                break;

#if defined(__linux__)
            case 'm':
                gOperation = MountPackage;
                gMountPoint = optarg;
                if (!std::filesystem::is_directory(gMountPoint))
                {
                    fprintf(stderr, "Error: mount point '%s' doesn't exist or is not a directory\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
#endif // defined(__linux__)

            case 'n':
                gVerifyPackage = false;
                break;

            case 'e':
                gEnableCertificateTimeCheck = true;
                break;

            case 'd':
                gCertificatesDir = optarg;
                if (!std::filesystem::is_directory(gCertificatesDir))
                {
                    fprintf(stderr, "Warning: certificates path '%s' doesn't exist or is not a directory\n", optarg);
                }
                break;

            case 'c':
                gCertificates.emplace_back(optarg);
                break;

            case '?':
            default:
                exit(EXIT_FAILURE);
        }
    }

    // extra arguments are interpreted as files to extract from the package
    for (int i = optind; i < argc; i++)
    {
        gFilesFilter.emplace(argv[i]);
    }
}

// -----------------------------------------------------------------------------
/*!
    Performs a full verification of the package.

 */
static bool verifyPackage(const Package &package)
{
    auto result = package.verify();
    if (!result)
    {
        fprintf(stderr, "Error: failed to verify package - %s\n", result.error().what());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Just list the contents of the package, optionally filtering by the given
    \a files list.

    The output tries to match that of the tar format.

    This doesn't do a full verification of the package, it just lists the files.

 */
static bool listPackage(const Package &package, const std::set<std::filesystem::path> &files)
{
    auto reader = PackageReader::create(package);
    if (reader.hasError())
    {
        fprintf(stderr, "Error: failed to create package reader - %s\n", reader.error().what());
        return false;
    }

    std::optional<PackageEntry> entry;
    while ((entry = reader.next()) != std::nullopt)
    {
        if (!files.empty() && files.find(entry->path()) == files.end())
            continue;

        char permissions[11] = "----------";
        switch (entry->type())
        {
            case std::filesystem::file_type::directory:
                permissions[0] = 'd';
                break;
            case std::filesystem::file_type::symlink:
                permissions[0] = 'l';
                break;
            case std::filesystem::file_type::character:
                permissions[0] = 'c';
                break;
            case std::filesystem::file_type::block:
                permissions[0] = 'b';
                break;
            default:
                break;
        }

        const auto perms = entry->permissions();
        if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::owner_read)
            permissions[1] = 'r';
        if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::owner_write)
            permissions[2] = 'w';
        if ((perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::owner_exec)
            permissions[3] = 'x';
        if ((perms & std::filesystem::perms::group_read) == std::filesystem::perms::group_read)
            permissions[4] = 'r';
        if ((perms & std::filesystem::perms::group_write) == std::filesystem::perms::group_write)
            permissions[5] = 'w';
        if ((perms & std::filesystem::perms::group_exec) == std::filesystem::perms::group_exec)
            permissions[6] = 'x';
        if ((perms & std::filesystem::perms::others_read) == std::filesystem::perms::others_read)
            permissions[7] = 'r';
        if ((perms & std::filesystem::perms::others_write) == std::filesystem::perms::others_write)
            permissions[8] = 'w';
        if ((perms & std::filesystem::perms::others_exec) == std::filesystem::perms::others_exec)
            permissions[9] = 'x';

        time_t t = entry->modificationTime();
        char modTime[128] = { 0 };
        std::strftime(modTime, sizeof(modTime), "%b %e %H:%M %Y", std::localtime(&t));

        printf("%-10s %4d/%-4d %8zu %s ", permissions, entry->ownerId(), entry->groupId(), entry->size(), modTime);

        if (entry->isSymLink())
        {
            char linkTarget[PATH_MAX];
            ssize_t linkSize = entry->read(linkTarget, PATH_MAX - 1);
            if (linkSize < 0)
                strcpy(linkTarget, "???");
            else
                linkTarget[linkSize] = '\0';

            printf("%s -> %s\n", entry->path().c_str(), linkTarget);
        }
        else
        {
            printf("%s\n", entry->path().c_str());
        }
    }

    if (reader.hasError())
    {
        fprintf(stderr, "Error: failed to read package - %s\n", reader.error().what());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Extracts the package to the given output directory, optionally filtering by
    the list of files.

    TODO: Add options for overwriting existing files, preserving permissions, etc.
    To match tar behaviour: \see https://www.gnu.org/software/tar/manual/html_node/Dealing-with-Old-Files.html

 */
static bool extractPackage(const Package &package, const std::set<std::filesystem::path> &files,
                           const std::filesystem::path &outputDir)
{
    int outputDirFd = open(outputDir.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (!outputDirFd)
    {
        fprintf(stderr, "Error: failed to open output directory '%s' - %m\n", outputDir.c_str());
        return false;
    }

    auto result = package.extractTo(outputDirFd);
    if (!result)
    {
        fprintf(stderr, "Error: failed to extract package - %s\n", result.error().what());
    }

    close(outputDirFd);

    return !result.isError();
}

#if defined(__linux__)
// -----------------------------------------------------------------------------
/*!
    Attempts to mount the package at the given mount point.

 */
static bool mountPackage(const Package &package, const std::filesystem::path &mountPoint)
{
    auto mountResult = package.mount(mountPoint);
    if (mountResult.is_error())
    {
        fprintf(stderr, "Error: failed to mount package - %s\n", mountResult.error().what());
        return false;
    }

    std::cout << "Package mounted successfully at " << mountResult->mountPoint() << std::endl;
    std::cout << "DM device name: " << mountResult->volumeName() << std::endl;
    std::cout << "DM device uuid: " << mountResult->volumeUuid() << std::endl;

    // The mount was successful, so we can detach the mount point so it doesn't get unmounted when the PackageMount
    // object is destroyed.
    mountResult->detach();

    return true;
}
#endif

// -----------------------------------------------------------------------------
/*!
    Attempts to extract a single file from the package and writes it to the given
    file descriptor.

 */
static bool extractSingleFileTo(const Package &package, const std::filesystem::path &filePath, int targetFd)
{
    auto reader = PackageReader::create(package);
    if (reader.hasError())
    {
        fprintf(stderr, "Error: failed to create package reader - %s\n", reader.error().what());
        return false;
    }

    std::optional<PackageEntry> entry;
    while ((entry = reader.next()) != std::nullopt)
    {
        if (!entry->isDirectory() && (entry->path() == filePath))
        {
            char buffer[4096];
            ssize_t bytesRead;
            Error err;

            while ((bytesRead = entry->read(buffer, sizeof(buffer), &err)) > 0)
            {
                if (write(targetFd, buffer, bytesRead) < 0)
                {
                    fprintf(stderr, "Error: failed to write to target file - %m\n");
                    return false;
                }
            }

            if (bytesRead < 0)
            {
                fprintf(stderr, "Error: failed to read file '%s' from package - %s\n", filePath.c_str(), err.what());
                return false;
            }

            return true;
        }
    }

    fprintf(stderr, "Error: file '%s' not found in the package\n", filePath.c_str());
    return false;
}

// -----------------------------------------------------------------------------
/*!
    Attempts to extract some auxiliary metadata file from the package and writes
    it to the given \a targetFd file descriptor.

*/
static bool extractAuxMetaDataTo(const Package &package, std::string_view name, int targetFd)
{
    auto auxFile = package.auxMetaDataFile(name);
    if (auxFile.is_error())
    {
        fprintf(stderr, "Error: failed to read auxiliary metadata file '%s' - %s\n", name.data(), auxFile.error().what());
        return false;
    }

    const auto contents = auxFile->readAll();
    if (!contents)
    {
        fprintf(stderr, "Error: failed to read auxiliary metadata file '%s' - %s\n", name.data(),
                contents.error().what());
        return false;
    }
    if (contents->empty())
    {
        fprintf(stderr, "Error: auxiliary metadata file '%s' is empty\n", name.data());
        return false;
    }

    if (write(targetFd, contents->data(), contents->size()) < 0)
    {
        fprintf(stderr, "Error: failed to write auxiliary metadata file '%s' - %m\n", name.data());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Dumps all the package meta data to stdout in JSON format.

 */
static bool dumpMetadata(const Package &package, MetaDataDumpFormat format)
{
    const auto packageFormat = package.format();
    if ((packageFormat != Package::Format::Widget) && (packageFormat != Package::Format::Ralf))
    {
        fprintf(stderr, "Error: unknown or unsupported package format (%d)\n", int(packageFormat));
        return false;
    }

    if (!gForceMetadataGeneration)
    {
        if ((packageFormat == Package::Format::Widget) && (format == MetaDataDumpFormat::Xml))
            format = MetaDataDumpFormat::Raw;
        if ((packageFormat == Package::Format::Ralf) && (format == MetaDataDumpFormat::Json))
            format = MetaDataDumpFormat::Raw;
    }

    if (format == MetaDataDumpFormat::Raw)
    {
        // Depending on the package type, the meta-data may be stored in the package itself or as an auxiliary metadata
        // file.  In the case of W3C widgets this is the config.xml file, in the case of OCI packages this is the
        // "application/vnd.rdk.package.config.v1+json" aux file.
        if (packageFormat == Package::Format::Widget)
        {
            return extractSingleFileTo(package, "config.xml", STDOUT_FILENO);
        }
        else if (packageFormat == Package::Format::Ralf)
        {
            return extractAuxMetaDataTo(package, "application/vnd.rdk.package.config.v1+json", STDOUT_FILENO);
        }
    }
    else
    {
        const auto metadata = package.metaData();
        if (!metadata)
        {
            fprintf(stderr, "Error: failed to read package meta data - %s\n", metadata.error().what());
            return false;
        }

        if (format == MetaDataDumpFormat::Json)
        {
            std::cout << metaDataToConfigSpec(metadata.value()) << std::endl;
        }
        else if (format == MetaDataDumpFormat::Xml)
        {
            std::cout << metaDataToConfigXml(metadata.value()) << std::endl;
        }
        else
        {
            fprintf(stderr, "Error: unknown metadata dump format %d\n", int(format));
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    Dumps details on the media types of the auxiliary metadata files in the package,
    or extracts a specific aux metadata file if the \a format argument is given.

*/
static bool dumpAuxMetadata(const Package &package, const std::optional<std::string> &mediaType)
{
    const auto result = package.auxMetaDataKeys();
    if (!result)
    {
        fprintf(stderr, "Error: failed to read auxiliary metadata keys - %s\n", result.error().what());
        return false;
    }

    const auto &keys = result.value();

    if (!mediaType)
    {
        if (keys.empty())
        {
            std::cout << "No auxiliary metadata files found for given package" << std::endl;
        }
        else
        {
            for (const auto &key : keys)
                std::cout << key << std::endl;
        }

        return true;
    }
    else
    {
        if (keys.count(mediaType.value()) == 0)
        {
            fprintf(stderr, "Error: auxiliary metadata file with media type '%s' not found in the package\n",
                    mediaType->c_str());
            return false;
        }

        return extractAuxMetaDataTo(package, mediaType.value(), STDOUT_FILENO);
    }
}

// -----------------------------------------------------------------------------
/*!
    Displays the certificates used for signing the package.

    TODO: add information on the CA certificate used for verification.
    TODO: add information on any signing annotations used.

 */
static bool dumpSignatureInfo(const Package &package)
{
    const auto result = package.signingCertificates();
    if (!result)
    {
        fprintf(stderr, "Error: failed to read package signing certificates - %s\n", result.error().what());
        return false;
    }

    const auto &signingCerts = result.value();
    if (signingCerts.empty())
    {
        fprintf(stderr, "No signing certificates found for given package\n");
        return true;
    }

    for (const auto &cert : signingCerts)
    {
        std::cout << cert << std::endl;
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!

 */
int main(int argc, char *argv[])
{
    parseArgs(argc, argv);

    // check an operation was supplied
    if (gOperation == NotSpecified)
    {
        fprintf(stderr, "Error: you must specify either '-t', '-x', '-m' or '-M' operation mode\n");
        return EXIT_FAILURE;
    }

    // set the stdout logger
    setLogHandler(stdoutLogHandler);

    // set the log level
    setLogLevel(nullptr, gLogLevel);

    // set up the verification credentials
    std::optional<VerificationBundle> creds;
    if (gVerifyPackage)
    {
        Error err;
        creds = VerificationBundle();

        if (gCertificates.empty())
        {
            std::error_code errorCode;
            std::filesystem::directory_options options = std::filesystem::directory_options::none;
            for (auto const &dirEntry : std::filesystem::directory_iterator(gCertificatesDir, options, errorCode))
            {
                if (dirEntry.is_regular_file())
                {
                    gCertificates.emplace_back(dirEntry.path());
                }
            }
        }

        std::list<Certificate> certs;
        for (auto const &certPath : gCertificates)
        {
            auto result = Certificate::loadFromFile(certPath, Certificate::EncodingFormat::PEM);
            if (!result)
            {
                fprintf(stderr, "Warning: failed to process cert '%s' - %s\n", certPath.c_str(), result.error().what());
            }
            else
            {
                certs.emplace_back(std::move(result.value()));
            }
        }

        if (certs.empty())
            fprintf(stderr, "Warning: verification is enabled but no certificates loaded\n");
        else
            creds->setCertificates(certs);
    }

    // sanity check the input package
    if (gPackageFilePath.empty())
    {
        fprintf(stderr, "Error: missing source package, need -f argument\n");
        return EXIT_FAILURE;
    }

    int packageFd = open(gPackageFilePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (packageFd < 0)
    {
        fprintf(stderr, "Error: failed to open '%s' for reading - %m\n", gPackageFilePath.c_str());
        return EXIT_FAILURE;
    }

    // attempt to open the package
    Result<Package> package;
    if (creds)
    {
        Package::OpenFlags openFlags = Package::OpenFlags::None;
        if (gEnableCertificateTimeCheck)
            openFlags |= Package::OpenFlag::CheckCertificateExpiry;

        package = Package::open(packageFd, creds.value(), openFlags);
    }
    else
    {
        package = Package::openWithoutVerification(packageFd, Package::OpenFlags::None);
    }

    if (package.is_error())
    {
        fprintf(stderr, "Error: failed to open package '%s' - %s\n", gPackageFilePath.c_str(), package.error().what());
        close(packageFd);
        return EXIT_FAILURE;
    }

    //
    bool result;
    switch (gOperation)
    {
        case VerifyPackage:
            result = verifyPackage(package.value());
            break;

        case ListContents:
            result = listPackage(package.value(), gFilesFilter);
            break;

        case ExtractPackage:
            result = extractPackage(package.value(), gFilesFilter, gOutputDirectory);
            break;

#if defined(__linux__)
        case MountPackage:
            result = mountPackage(package.value(), gMountPoint);
            break;
#endif // defined(__linux__)

        case DumpMetadata:
            result = dumpMetadata(package.value(), gDumpMetaDataFormat);
            break;

        case DumpAuxMetadata:
            result = dumpAuxMetadata(package.value(), gAuxMetaDataMediaType);
            break;

        case DumpSignatureInfo:
            result = dumpSignatureInfo(package.value());
            break;

        case NotSpecified:
        default:
            result = false;
            break;
    }

    // close the package
    close(packageFd);
    return result ? EXIT_SUCCESS : EXIT_FAILURE;
}
