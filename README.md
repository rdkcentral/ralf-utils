# RALF Utils - Library for working with RDK / EntertainmentOS widget and OCI artifact packages.

## Description
Library and tooling for working with widget (`.wgt`) files and OCI artifact packages as used on EntertainmentOS platforms
for application and runtime distribution.

Widget files were nominally based on the W3C Widget format, and are a zip file containing a signature file
(`signature1.xml`), a manifest file (`config.xml`) and the application files.  However, a [new format][2] has been
proposed base on the OCI Artifact specification, where metadata is stored in JSON config file(s) and application / runtime
files can be stored in compressed tar format or as an EROFS filesystem image, protected by a dm-verity hash tree and a
signed root hash.  This library supports both formats.

Because "widget" is a term from the W3C format, the term used within the library is "package" to refer to both a zip / W3C
based widget and an OCI Artifact base package.

It is intended the library will provide simple APIs for:
* Verifying the packages against one or more public keys / certificates.
* Extracting the contents of the package (with verification).
* Extracting and parsing the package metadata (`config.xml` or `config.json`) file(s).
* Mounting EROFS image packages.

The library will NOT - at least initially - provide APIs for:
* Creating and signing packages.
* Converting packages between formats (e.g. zip to OCI Artifact).

## Languages

### C++
The code is written in C++ and uses the C++17 standard.  It's official API is C++ and it's expected to be used primarily
in C++ projects.

### Rust
In the future the library will provide Rust bindings to the C++ API.

### Python
In the future the library will provide Python bindings to the C++ API.

### Javascript / WASM
In the future the library will be made buildable to WebAssembly and will provide Javascript bindings to the C++ API.


## Dependencies
The library has the following dependencies:
* `libarchive` - for reading zip and tar files.
* `libxml2` - for parsing XML files.
* `openssl` - for cryptographic operations.
* `lz4` - for decompressing EROFS images _(may look at importing this as source if licensing is not an issue)_.

Internally the library uses the excellent [nlohmann json][3] library for JSON parsing and serialization.


## Building on Linux
The library uses CMake for building.

Run cmake to configure the build system.  This will create a `build` directory and generate the necessary files for
building the library.  The `-S` option specifies the source directory (the current directory) and the `-B` option
specifies the build directory.
```
mkdir build
cmake -S . -B build
```

To build the library, run the following command:
```
cmake --build build
```

### Building on macOS
It's the same process as above, but you may need to install dependencies using `brew` or `ports` first.  For example:
```
brew install libarchive libxml2 openssl lz4
```
And you may need to add the following options to the `cmake` command to find the libraries:
```
-DLibArchive_ROOT=/opt/homebrew/opt/libarchive/
```

### Building with vcpkg
You can build on Linux, Windows or macOS using the [vcpkg](https://vcpkg.io/) package manager to install dependencies. 
First, install vcpkg by following the instructions on the [vcpkg GitHub page][4].

Then, run cmake with the `-DCMAKE_TOOLCHAIN_FILE` option to point to the vcpkg toolchain file.  For example:
```
mkdir vcpkg-build
cmake -S . -B vcpkg-build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build vcpkg-build
```

### Building and Running Unit Tests
This repo uses [git LFS][1] for storing test data files.  If you have not already done so, run the following command to
pull down the LFS files:
```
git lfs pull
```

Unit tests are not built by default.  To build the tests, add the following `-DBUILD_UNIT_TESTS:BOOL=ON`
cmake option.  For example:

```
cmake -S . -B build -DRALF_UTILS_BUILD_UNIT_TESTS:BOOL=ON
```

Then build and run the tests:
```
cmake --build build
ctest --test-dir build/test
```

[1]: https://docs.github.com/en/repositories/working-with-files/managing-large-files/installing-git-large-file-storage
[2]: https://github.com/rdkcentral/oci-package-spec
[3]: https://github.com/nlohmann/json/
[4]: https://learn.microsoft.com/en-gb/vcpkg/get_started/get-started?pivots=shell-bash
