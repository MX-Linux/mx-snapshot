#!/usr/bin/env bash
# Copyright (C) 2017-2026 MX Authors. License: GPL-3.0-or-later
set -euo pipefail
unset CDPATH
shopt -s nullglob

die() { echo "build: $*" >&2; exit 1; }
usage() {
    cat <<'HELP'
Usage: build [OPTIONS]
  -d, --debug       Debug build (default: Release)
  -c, --clang       Use Clang, including for Debian packages
      --clean       Clean the selected build tree before configuring
      --clean-only  Clean and exit (alias: clean)
      --configure   Configure only (alias: configure)
      --build-only  Build an existing configuration (aliases: build, make)
      make-clean    Clean compiled targets in the existing configuration
      all, fresh    Configure and build, optionally starting clean
  -t, --tests       Enable and run CTest (also --test)
  -j, --jobs N      Set the number of build jobs
      --asan        Enable AddressSanitizer in a separate build tree
      --gui-only    Build the GUI in projects with split GUI/CLI targets
      --cli-only    Build the CLI in projects with split GUI/CLI targets
      --debian      Build unsigned Debian packages into debs/
      --arch        Build a local Arch package into build/arch/
      --nodeps      Skip Arch dependency checks (for local cross-distro builds)
  -h, --help        Show this help
HELP
}

mode=normal
type=Release
compiler=default
clean=false
tests=false
asan=false
nodeps=false
component=
jobs=
set_mode() {
    [[ $mode == normal ]] || die "choose one build mode"
    mode=$1
}
while (($#)); do
    case $1 in
        -h|--help|help) usage; exit ;;
        -d|--debug) type=Debug ;;
        -c|--clang) compiler=clang ;;
        --clean|fresh|--fresh) clean=true ;;
        --clean-only|clean) set_mode clean-only; clean=true ;;
        --configure|configure) set_mode configure ;;
        --build-only|build|make) set_mode build-only ;;
        all) ;;
        make-clean) set_mode make-clean ;;
        --debian|--arch) set_mode "${1#--}" ;;
        -t|--tests|--test) tests=true ;;
        --asan) asan=true ;;
        --nodeps) nodeps=true ;;
        --gui-only|--cli-only)
            [[ -z $component ]] || die "choose one GUI/CLI target"
            component=${1#--} ;;
        -j|--jobs)
            [[ ${2:-} =~ ^[1-9][0-9]*$ ]] || die "--jobs requires a positive integer"
            jobs=$2; shift ;;
        *) die "unknown argument: $1" ;;
    esac
    shift
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -f $script_dir/CMakeLists.txt ]]; then
    cd -- "$script_dir"
else
    cd -- "$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
fi
root=$PWD
dir=build
[[ $compiler != clang ]] || dir+=-clang
[[ $type != Debug ]] || dir+=-debug
[[ $asan == false ]] || dir+=-asan
[[ -z $component ]] || dir+=-"$component"

cleanup() {
    # Older builds wrote Arch packages directly to build/. Current builds use build/arch/.
    if [[ $dir == build && ! -L build && -d build ]]; then
        find build -mindepth 1 -maxdepth 1 ! -name arch ! -name '*.pkg.tar' ! -name '*.pkg.tar.*' -exec rm -rf -- {} +
        rmdir --ignore-fail-on-non-empty -- build
    else
        rm -rf -- "$dir"
    fi
    rm -f -- debian/*.debhelper.log debian/*.substvars debian/files translations/*.qm
    rm -rf -- debian/.debhelper obj-*/
    if [[ -f debian/control ]]; then
        while read -r package; do
            [[ $package =~ ^[a-z0-9][a-z0-9+.-]+$ ]] && rm -rf -- "debian/$package"
        done < <(sed -n 's/^Package:[[:space:]]*//p' debian/control)
    fi
    if [[ -f src/version.h ]] && git check-ignore -q -- src/version.h; then rm -- src/version.h; fi
    return 0
}

build_debian() {
    [[ -f debian/rules && -f debian/changelog ]] || die "Debian packaging is missing"
    local source version arch changes log
    source=$(dpkg-parsechangelog -SSource)
    version=$(dpkg-parsechangelog -SVersion)
    arch=$(dpkg-architecture -qDEB_HOST_ARCH)
    changes=../${source}_${version#*:}_${arch}.changes
    log=${changes%.changes}.build
    local args=(-us -uc)
    [[ $compiler != clang ]] || args+=(-eCC=clang -eCXX=clang++)
    [[ -z $jobs ]] || args+=("-j$jobs")
    command -v dcmd >/dev/null || die "dcmd from devscripts is required"
    debuild "${args[@]}"
    [[ -s $changes ]] || die "missing changes manifest: $changes"
    if [[ $source == mx-viewer ]]; then
        # Its source package must contain only files tracked by Git.
        local archive=../${source}_${version#*:}.tar.xz files tracked extra
        files=$(tar -tf "$archive" | sed -n 's|^[^/]*/||p' | sed '/^$/d; /\/$/d' | LC_ALL=C sort -u)
        tracked=$(git -c core.quotePath=false ls-files | LC_ALL=C sort -u)
        extra=$(LC_ALL=C comm -23 <(printf '%s\n' "$files") <(printf '%s\n' "$tracked"))
        [[ -z $extra ]] || die "untracked files in $archive: $extra"
    fi
    mkdir -p debs
    dcmd mv -- "$changes" debs/
    if [[ -f $log ]]; then mv -- "$log" debs/; fi
    cleanup
}

build_arch() (
    command -v makepkg >/dev/null || die "--arch requires makepkg"
    local recipe work tree stage version metadata source prefix archive github
    for recipe in arch . aur; do [[ ! -f $recipe/PKGBUILD ]] || break; done
    [[ -f $recipe/PKGBUILD ]] || die "no PKGBUILD in arch/, the project root, or aur/"
    work=$(mktemp -d)
    trap 'rm -rf -- "$work"' EXIT
    stage=$work/recipe
    mkdir -p "$stage" "$root/build/arch"
    # Snapshot tracked edits without changing the developer tree or index.
    tree=$(git stash create)
    tree=${tree:-HEAD}
    version=
    if [[ -f debian/changelog ]]; then
        version=$(sed -n '1s/^[^(]*(\([^)]*\)).*/\1/p' debian/changelog)
        version=${version#*:}; version=${version%-*}
    fi
    export PKGDEST="$root/build/arch" BUILDDIR="$work/build" SRCDEST="$work/sources"
    mkdir -p "$SRCDEST"
    if [[ -n $jobs ]]; then export MAKEFLAGS="-j$jobs" CMAKE_BUILD_PARALLEL_LEVEL="$jobs"; fi
    if [[ $recipe == . ]]; then
        git archive "$tree" -- . ':!debs' | tar -x -C "$stage"
        [[ -z $version ]] || export PKGVER="$version"
    else
        # Copy recipe inputs only. Makepkg may leave package caches beside the recipe.
        for file in "$recipe"/*; do
            [[ -f $file ]] || continue
            case $file in *.pkg.tar.*|*.src.tar.*) continue ;; esac
            cp -- "$file" "$stage/"
        done
        if [[ -n $version ]]; then sed -i "s/^pkgver=.*/pkgver=$version/" "$stage/PKGBUILD"; fi
        metadata=$(cd "$stage" && makepkg --printsrcinfo)
        source=$(awk '$1=="source" {print $3; exit}' <<< "$metadata")
        github='^https://github\.com/[^/]+/([^/]+)/archive/refs/tags/([^/]+)\.tar\.gz$'
        case $source in
            *_*.tar.xz) archive=$source; prefix=src ;;
            *-source.tar) archive=$source; prefix=${source%.tar} ;;
            *)
                if [[ ${source#*::} =~ $github ]]; then
                    prefix=${BASH_REMATCH[1]}-${BASH_REMATCH[2]#v}
                    archive=${source##*/}
                    [[ $source != *::* ]] || archive=${source%%::*}
                else
                    archive=
                fi ;;
        esac
        if [[ -n $archive ]]; then
            [[ $archive != */* && $archive != .* ]] || die "invalid source archive: $archive"
            git archive --prefix="$prefix/" "$tree" -- . ':!debs' > "$work/tree.tar"
            case $archive in
                *.gz) gzip -c "$work/tree.tar" > "$SRCDEST/$archive" ;;
                *.xz) xz -c "$work/tree.tar" > "$SRCDEST/$archive" ;;
                *) mv -- "$work/tree.tar" "$SRCDEST/$archive" ;;
            esac
            # Replace just the working-tree archive checksum, preserving auxiliary checksums.
            local checksum
            checksum=$(sha256sum "$SRCDEST/$archive"); checksum=${checksum%% *}
            sed -Ei "0,/^sha256sums=\(/s/^(sha256sums=\([[:space:]]*)['\"]?[^ )'\"]+['\"]?/\1'$checksum'/" "$stage/PKGBUILD"
        fi
    fi
    cd -- "$stage"
    local flags=()
    if [[ $nodeps == true ]]; then flags+=(--nodeps); fi
    makepkg --verifysource "${flags[@]}"
    makepkg -f "${flags[@]}"
    makepkg --packagelist
)

[[ $nodeps == false || $mode == arch ]] || die "--nodeps requires --arch"
if [[ $mode == arch || $mode == debian ]]; then
    [[ $type == Release && $tests == false && $clean == false && $asan == false && -z $component ]] || die "package mode cannot be combined with CMake options"
    if [[ $mode == arch ]]; then
        [[ $compiler == default ]] || die "--arch uses the recipe's compiler"
        build_arch
    else
        build_debian
    fi
    exit
fi

[[ -f CMakeLists.txt ]] || die "no CMakeLists.txt in $root"
[[ $mode != build-only || $clean == false ]] || die "--clean conflicts with --build-only"
[[ $mode != clean-only || $tests == false ]] || die "--clean-only cannot run tests"
if [[ $clean == true ]]; then cleanup; fi
[[ $mode != clean-only ]] || exit 0

if [[ $mode == make-clean ]]; then
    cmake --build "$dir" --target clean
    exit
fi
if [[ $mode != build-only ]]; then
    args=(-S . -B "$dir" -G Ninja "-DCMAKE_BUILD_TYPE=$type" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
    if grep -Eq '^[[:space:]]*project\(update-notifier-qt([[:space:]]|\))' CMakeLists.txt; then args+=(-DCMAKE_INSTALL_PREFIX=/usr); fi
    if [[ $compiler == clang ]]; then args+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++); fi
    if grep -Eiq 'option\(WARNINGS_AS_ERRORS\b' CMakeLists.txt; then args+=(-DWARNINGS_AS_ERRORS=ON); fi
    if [[ -n $component ]]; then
        grep -Eiq 'option\(BUILD_GUI\b' CMakeLists.txt || die "this project does not define BUILD_GUI"
        grep -Eiq 'option\(BUILD_CLI\b' CMakeLists.txt || die "this project does not define BUILD_GUI and BUILD_CLI"
        if [[ $component == cli-only ]]; then args+=(-DBUILD_GUI=OFF -DBUILD_CLI=ON)
        else args+=(-DBUILD_GUI=ON -DBUILD_CLI=OFF); fi
    fi
    if [[ $asan == true ]]; then
        args+=("-DCMAKE_CXX_FLAGS=-fsanitize=address -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address)
    fi
    if [[ $tests == true ]]; then
        args+=(-DBUILD_TESTING=ON)
        for opt in BUILD_TESTS ENABLE_TESTS; do
            if grep -Eiq "option\($opt\b" CMakeLists.txt; then args+=("-D$opt=ON"); fi
        done
    fi
    cmake "${args[@]}"
    [[ $mode != configure ]] || exit 0
else
    [[ -f $dir/CMakeCache.txt ]] || die "$dir has not been configured"
fi
cmake --build "$dir" --parallel ${jobs:+"$jobs"}
if [[ $tests == true ]]; then ctest --test-dir "$dir" --output-on-failure --no-tests=error; fi
