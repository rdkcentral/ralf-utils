# Contributing to RALF Utils

Thank you for your interest in contributing to RALF Utils! This document provides guidelines and information for contributors.

## Table of Contents
- [Overview](#overview)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Building the Project](#building-the-project)
- [Running Tests](#running-tests)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Reporting Issues](#reporting-issues)

## Overview

If you would like to contribute code to this project you can do so through GitHub by forking the repository and
sending a pull request.

Before RDK accepts your code into the project you must sign the RDK Contributor License Agreement (CLA).

## Getting Started

### Prerequisites

Before contributing, ensure you have the following dependencies installed:

- **CMake** (version 3.12 or higher)
- **C++ compiler** with C++17 support (GCC 7+, Clang 5+, or MSVC 2017+)
- **Dependencies:**
  - `libarchive` - for reading zip and tar files
  - `libxml2` - for parsing XML files
  - `openssl` - for cryptographic operations
  - `lz4` - for decompressing EROFS images

### Forking and Cloning

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/your-username/ralf-utils.git
   cd ralf-utils
   ```

## Development Environment

### Using vcpkg (Recommended)

The project supports vcpkg for dependency management. To use vcpkg:

1. Install vcpkg if you haven't already
2. Build with vcpkg integration:
   ```bash
   mkdir cmake-build-vcpkg-debug
   cd cmake-build-vcpkg-debug
   cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake ..
   make
   ```

### Manual Dependency Management

If you prefer to manage dependencies manually, ensure all required libraries are installed and discoverable by CMake.

## Building the Project

### Standard Build

```bash
mkdir build
cd build
cmake ..
make
```

### Build Options

The project provides several CMake options:

- `BUILD_SHARED_LIBS=ON/OFF` - Build as shared library (default: ON)
- `CMAKE_BUILD_TYPE=Debug/Release` - Build type
- `RALF_UTILS_BUILD_UNIT_TESTS=ON/OFF` - Enable/disable building tests (default: OFF)
- `RALF_UTILS_BUILD_TOOLS=ON/OFF` - Enable/disable building command line tools (default: ON)
- Various sanitizer options for debugging

## Running Tests

### Unit Tests

The project uses GoogleTest for unit testing:

```bash
cd cmake-build-unittests
cmake -DCMAKE_BUILD_TYPE=Debug -DRALF_UTILS_BUILD_UNIT_TESTS:BOOL=ON ..
make
ctest
```

### Test Organization

Tests are organized by component:
- `test/core/` - Core library tests
- `test/crypto/` - Cryptographic functionality tests
- `test/dm-verity/` - dm-verity related tests
- `test/erofs/` - EROFS filesystem tests

## Coding Standards

### C++ Style Guidelines

- **Standard**: C++17
- **Exceptions**: Do not use exceptions for any public APIs
- **Formatting**: Use clang-format with the project's configuration
- **Naming Conventions**:
  - Classes: `PascalCase` (e.g., `PackageReader`)
  - Functions/Methods: `camelCase` (e.g., `readPackage`)
  - Variables: `camelCase` (e.g., `packageData`)
  - Constants: `UPPER_CASE` (e.g., `MAX_PACKAGE_SIZE`)
  - Files: `PascalCase` (e.g., `PackageReader.h`)

### Code Quality

- Write self-documenting code with clear variable and function names
- Add comments for complex algorithms or business logic
- Use modern C++ features appropriately (smart pointers, RAII, etc.)
- Follow SOLID principles where applicable
- Ensure thread safety where required

### Formatting

Run clang-format before submitting:

```bash
find lib include test tools -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

###  Linting

It's recommended to build with clang-tidy enabled to catch common issues, for example:

```bash
cd clang-tidy-build
cmake -DRALF_UTILS_CLANG_TIDY:BOOL=ON -S . -B clang-tidy-build
cmake --build clang-tidy-build
```

## Submitting Changes

### Pull Request Process

1. **Create a branch** for your feature or fix:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following the coding standards

3. **Add tests** for new functionality

4. **Run the test suite** to ensure nothing is broken:
   ```bash
   cd cmake-build-unittests
   make && ctest
   ```

5. **Format your code**:
   ```bash
   find lib include test tools -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   ```

6. **Commit your changes** with a clear commit message:
   ```bash
   git commit -m "Add feature: brief description of what was added"
   ```

7. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

8. **Create a Pull Request** on GitHub

### Code Review Process

- All changes require review before merging
- Address reviewer feedback promptly
- Ensure CI/CD checks pass
- Maintain clean commit history (squash if necessary)

## Reporting Issues

### Bug Reports

When reporting bugs, please include:

- **Description**: Clear description of the issue
- **Steps to reproduce**: Detailed steps to reproduce the problem
- **Expected behavior**: What you expected to happen
- **Actual behavior**: What actually happened
- **Environment**: OS, compiler version, dependencies
- **Logs**: Relevant error messages or logs

### Feature Requests

For feature requests, please provide:

- **Use case**: Why is this feature needed?
- **Description**: Detailed description of the proposed feature
- **Implementation ideas**: Any thoughts on how it could be implemented

### Security Issues

For security-related issues, please email the maintainers directly rather than creating a public issue.

## Development Guidelines

### API Design

- Follow existing API patterns in the codebase
- Prefer composition over inheritance
- Use clear, descriptive names for public interfaces
- Document public APIs with Doxygen comments
- Avoid API / ABI breaking changes in minor releases (see [Policies/Binary Compatibility Issues With C++](https://community.kde.org/Policies/Binary_Compatibility_Issues_With_C%2B%2B))

### Error Handling

- Use the project's `Result<T>` type for fallible operations
- Provide meaningful error messages
- Follow RAII principles for resource management

### Dependencies

- Minimize external dependencies
- Use header-only libraries when possible
- Ensure all dependencies are properly licensed
- Document new dependencies in the README

### Performance

- Write efficient code, but prioritize readability
- Profile performance-critical sections
- Use appropriate data structures and algorithms
- Consider memory usage and allocation patterns

## Getting Help

- Create an issue for questions about the codebase
- Check existing issues and pull requests first
- Be specific about what you're trying to achieve

## License

By contributing to RALF Utils, you agree that your contributions will be licensed under the Apache License 2.0, the same license as the project.
