#!/bin/bash

# **********************************************************************
# * Copyright (C) 2015-2026 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This file is part of mx-snapshot.
# *
# * mx-snapshot is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * mx-snapshot is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with mx-snapshot.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

set -e

# Default values
BUILD_DIR="build"
BUILD_TYPE="Release"
USE_CLANG=false
CLEAN=false
DEBIAN_BUILD=false
ARCH_BUILD=false
BUILD_GUI=true
BUILD_CLI=true
USE_ASAN=false
BUILD_TESTS=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clang)
            USE_CLANG=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --debian)
            DEBIAN_BUILD=true
            shift
            ;;
        --arch)
            ARCH_BUILD=true
            shift
            ;;
        --gui-only)
            BUILD_GUI=true
            BUILD_CLI=false
            shift
            ;;
        --cli-only)
            BUILD_GUI=false
            BUILD_CLI=true
            shift
            ;;
        --asan)
            USE_ASAN=true
            shift
            ;;
        --tests)
            BUILD_TESTS=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -d, --debug     Build in Debug mode (default: Release)"
            echo "  -c, --clang     Use clang compiler"
            echo "  --clean         Clean build directory before building"
            echo "  --debian        Build Debian package"
            echo "  --arch          Build Arch Linux package"
            echo "  --gui-only      Build only GUI version (mx-snapshot)"
            echo "  --cli-only      Build only CLI version (iso-snapshot-cli)"
            echo "  --asan          Enable AddressSanitizer for memory debugging"
            echo "  --tests         Build and run the test suite"
            echo "  -h, --help      Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Build Debian package
if [ "$DEBIAN_BUILD" = true ]; then
    echo "Building Debian package..."
    debuild -us -uc

    echo "Creating debs directory and moving debian artifacts..."
    mkdir -p debs
    for artifact in ../{mx-snapshot,iso-snapshot-cli}_*.{deb,changes,dsc,tar.gz,tar.xz,tar.bz2,tar.lzma,tar.zst,buildinfo,build}; do
        [[ -f $artifact ]] || continue
        mv -- "$artifact" debs/
    done

    echo "Cleaning build directory and debian artifacts..."
    rm -rf "$BUILD_DIR"
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/mx-snapshot/ debian/iso-snapshot-cli/ obj-*/
    rm -f translations/*.qm src/version.h
    rm -rf pkg *.pkg.tar.zst

    echo "Debian package build completed!"
    echo "Debian artifacts moved to debs/ directory"
    exit 0
fi

# Build Arch Linux package
if [ "$ARCH_BUILD" = true ]; then
    echo "Building Arch Linux package..."

    # Check if makepkg is available
    if ! command -v makepkg &> /dev/null; then
        echo "Error: makepkg not found. Please install base-devel package."
        exit 1
    fi

    if [ ! -f PKGBUILD ]; then
        echo "Error: PKGBUILD not found; cannot build Arch package."
        exit 1
    fi

    if [ ! -f debian/changelog ]; then
        echo "Error: debian/changelog not found; cannot determine version for Arch build."
        exit 1
    fi

    ARCH_VERSION=$(sed -n '1{s/^[^(]*(\([^)]*\)).*/\1/p}' debian/changelog)
    if [ -z "$ARCH_VERSION" ]; then
        echo "Error: could not parse version from debian/changelog."
        exit 1
    fi
    echo "Using version ${ARCH_VERSION} from debian/changelog"

    ARCH_BUILDDIR=$(mktemp -d -p "${TMPDIR:-/tmp}" archpkgbuild.XXXXXX)
    trap 'rm -rf "$ARCH_BUILDDIR"' EXIT

    # Clean previous build artifacts
    rm -rf pkg *.pkg.tar.zst

    PKG_DEST_DIR="$PWD/build"
    mkdir -p "$PKG_DEST_DIR"
    if [ ! -w "$PKG_DEST_DIR" ]; then
        echo "Error: build directory is not writable: $PKG_DEST_DIR"
        exit 1
    fi

    # Build package (without --clean to preserve directories)
    BUILDDIR="$ARCH_BUILDDIR" PKGDEST="$PKG_DEST_DIR" PKGVER="$ARCH_VERSION" makepkg -f

    # Clean makepkg artifacts
    echo "Cleaning makepkg artifacts..."
    rm -rf pkg

    echo "Arch Linux package build completed!"
    echo "Package: $(ls "$PKG_DEST_DIR"/*.pkg.tar.zst 2>/dev/null || echo 'not found')"
    echo "Binary available at: $PWD/build/arch-package/mx-snapshot"
    exit 0
fi

# A build dir configured with gcc keeps its cached compiler and silently
# ignores a later compiler change; give --clang builds their own directory so
# the option always takes effect without wiping the default build.
if [ "$USE_CLANG" = true ] && [ "$BUILD_DIR" = "build" ]; then
    BUILD_DIR="build-clang"
fi

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory and debian artifacts..."
    rm -rf "$BUILD_DIR"
    rm -f debian/*.debhelper.log debian/*.substvars debian/files
    rm -rf debian/.debhelper/ debian/mx-snapshot/ debian/iso-snapshot-cli/ obj-*/
    rm -f translations/*.qm src/version.h
    for artifact in ../{mx-snapshot,iso-snapshot-cli}_*.{build,buildinfo}; do
        [[ -f $artifact ]] || continue
        rm -f -- "$artifact"
    done
    rm -rf pkg *.pkg.tar.zst
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure CMake with Ninja
echo "Configuring CMake with Ninja generator..."
CMAKE_ARGS=(
    -G Ninja
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DBUILD_GUI="$([ "$BUILD_GUI" = true ] && echo ON || echo OFF)"
    -DBUILD_CLI="$([ "$BUILD_CLI" = true ] && echo ON || echo OFF)"
    -DBUILD_TESTS="$([ "$BUILD_TESTS" = true ] && echo ON || echo OFF)"
)

if [ "$USE_CLANG" = true ]; then
    CMAKE_ARGS+=(-DUSE_CLANG=ON)
    echo "Using clang compiler"
fi

if [ "$USE_ASAN" = true ]; then
    CMAKE_ARGS+=(
        -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
    )
    echo "AddressSanitizer enabled"
fi

echo "Build configuration:"
echo "  GUI: $([ "$BUILD_GUI" = true ] && echo "enabled" || echo "disabled")"
echo "  CLI: $([ "$BUILD_CLI" = true ] && echo "enabled" || echo "disabled")"
echo "  Tests: $([ "$BUILD_TESTS" = true ] && echo "enabled" || echo "disabled")"
echo "  ASAN: $([ "$USE_ASAN" = true ] && echo "enabled" || echo "disabled")"

cmake "${CMAKE_ARGS[@]}"

# Build the project
echo "Building project with Ninja..."
cmake --build "$BUILD_DIR" --parallel

if [ "$BUILD_TESTS" = true ]; then
    echo "Running tests..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "Build completed successfully!"
if [ "$BUILD_GUI" = true ]; then
    echo "GUI executable: $BUILD_DIR/mx-snapshot"
fi
if [ "$BUILD_CLI" = true ]; then
    echo "CLI executable: $BUILD_DIR/iso-snapshot-cli"
fi
