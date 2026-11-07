# Maintainer: Adrian <adrian@mxlinux.org>
pkgname=mx-snapshot
pkgver=${PKGVER:-26.10.2}
pkgrel=1
pkgdesc="A tool for creating live ISO images from running systems"
arch=('x86_64' 'i686')
url="https://mxlinux.org"
license=('GPL3')
# openssl: installed-to-live-arch hashes the demo/root passwords with
# `openssl passwd -6` in reset-accounts mode (python3 is the fallback).
# syslinux: provides isohdpfx.bin, needed for make_isohybrid=yes (the
# shipped default) to actually produce a BIOS/USB-hybrid ISO.
depends=('qt6-base' 'polkit' 'squashfs-tools' 'xorriso' 'mx-iso-template-arch' 'lsb-release' 'openssl'
         'syslinux')
optdepends=('paru: install gazelle-installer from the AUR when missing'
            'xdg-user-dirs: resolve localized Documents/Downloads/etc. paths for exclusions')
makedepends=('cmake' 'ninja' 'qt6-tools')
conflicts=('mx-remaster-live-files')
replaces=('mx-remaster-live-files')
provides=('mx-remaster-live-files=1.0.0')
# /etc/mx-snapshot.conf and the exclude list are meant to be user-edited;
# without this, pacman would overwrite local changes on every upgrade.
backup=('etc/mx-snapshot.conf' 'etc/mx-snapshot-exclude.list')
_builddir=build/arch-package

source=()
sha256sums=()

build() {
    cd "${startdir}"

    # Reconfigure from a clean package-only directory, preserving developer builds.
    rm -rf "$_builddir"

    # Arch packaging is intentionally GUI-only.
    # iso-snapshot-cli is a Debian-only deliverable: on Debian it ships in its
    # own .deb so headless / CI workflows can install just the CLI tool. On
    # Arch we don't build it and don't install its scripts, polkit policies,
    # or rules. If you need a CLI build for development, override with
    # `-DBUILD_CLI=ON` via makepkg's CFLAGS/options or build directly with
    # cmake.
    cmake -G Ninja \
        -B "$_builddir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DARCH_BUILD=ON \
        -DBUILD_GUI=ON \
        -DBUILD_CLI=OFF

    cmake --build "$_builddir" --parallel
}

package() {
    cd "${startdir}"

    install -Dm755 "$_builddir/mx-snapshot" "${pkgdir}/usr/bin/mx-snapshot"

    install -dm755 "${pkgdir}/usr/share/mx-snapshot/locale"
    install -Dm644 "$_builddir"/*.qm "${pkgdir}/usr/share/mx-snapshot/locale/" 2>/dev/null || true

    # Arch packaging is GUI-only (BUILD_CLI=OFF above); install scripts only
    # under /usr/share/mx-snapshot/, not /usr/share/iso-snapshot-cli/.
    install -dm755 "${pkgdir}/usr/share/mx-snapshot/scripts"
    cp -a scripts-arch/* "${pkgdir}/usr/share/mx-snapshot/scripts/"

    install -dm755 "${pkgdir}/usr/lib/mx-snapshot"
    install -Dm755 "$_builddir/helper" "${pkgdir}/usr/lib/mx-snapshot/helper"
    install -Dm755 polkit/snapshot-lib "${pkgdir}/usr/lib/mx-snapshot/snapshot-lib"

    # GUI-only: install only the mx-snapshot policies. The iso-snapshot-cli
    # policies would be inert without the CLI binary on this distro.
    install -dm755 "${pkgdir}/usr/share/polkit-1/actions"
    for policy in polkit/*mx-snapshot*.policy; do
        install -Dm644 "$policy" "${pkgdir}/usr/share/polkit-1/actions/$(basename "$policy")"
    done

    install -Dm644 mx-snapshot.desktop "${pkgdir}/usr/share/applications/mx-snapshot.desktop"
    install -Dm644 icons/mx-snapshot.png "${pkgdir}/usr/share/icons/hicolor/64x64/apps/mx-snapshot.png"
    install -Dm644 icons/mx-snapshot.svg "${pkgdir}/usr/share/icons/hicolor/scalable/apps/mx-snapshot.svg"
    install -Dm644 icons/mx-snapshot.png "${pkgdir}/usr/share/pixmaps/mx-snapshot.png"

    install -Dm644 mx-snapshot.conf "${pkgdir}/etc/mx-snapshot.conf"
    install -Dm644 mx-snapshot-exclude-arch.list "${pkgdir}/etc/mx-snapshot-exclude.list"

    install -dm755 "${pkgdir}/usr/share/excludes"
    install -Dm644 mx-snapshot-exclude-arch.list "${pkgdir}/usr/share/excludes/mx-snapshot-exclude.list"

    install -Dm644 manual/mx-snapshot.8 "${pkgdir}/usr/share/man/man8/mx-snapshot.8"

    install -dm755 "${pkgdir}/usr/share/doc/mx-snapshot"
    if [ -d docs ]; then
        cp -r docs/* "${pkgdir}/usr/share/doc/mx-snapshot/" 2>/dev/null || true
    fi

    # Install live-files (replaces mx-remaster-live-files package)
    install -dm755 "${pkgdir}/usr/share/live-files"
    cp -a live-files/files live-files/general-files "${pkgdir}/usr/share/live-files/"
    ln -s /usr/lib/libostree/grub2-15_ostree \
        "${pkgdir}/usr/share/live-files/files/etc/grub.d/15_ostree"

    # Lets the unprivileged GUI take the single-instance lock: /run/lock is
    # root-only on Arch.
    install -Dm644 tmpfiles.d/mx-snapshot.conf "${pkgdir}/usr/lib/tmpfiles.d/mx-snapshot.conf"

    # Install changelog
    if [ -f debian/changelog ]; then
        gzip -c debian/changelog > "${pkgdir}/usr/share/doc/mx-snapshot/changelog.gz"
    fi

}
