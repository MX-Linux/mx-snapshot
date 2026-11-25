mx-snapshot
===================

[![latest packaged version(s)](https://repology.org/badge/latest-versions/mx-snapshot.svg)](https://repology.org/project/mx-snapshot/versions)
[![build result](https://build.opensuse.org/projects/home:mx-packaging/packages/mx-snapshot/badge.svg?type=default)](https://software.opensuse.org//download.html?project=home%3Amx-packaging&package=mx-snapshot)

Program for creating a live-CD from MX Linux and antiX running system

JUST TO CLARIFY, this program is meant for MX Linux and antiX it won't work on another other system without considerable modifications because other systems don't have the infrastructure needed to run this program. Don't try to install the deb it won't work and might ruin your system.

# SYNOPSIS

mx-snapshot \[options\]

# DESCRIPTION

Program used for creating functional ISO images from the running system.
You can later on install/burn the ISO image on a USB flashdrive or a DVD
(depending on the size of the image). Use MX Live USB Maker (GUI) or
live-usb-maker (CLI tool) to burn the resulting image.

The tool has basically two different modes: - Preserving accounts --
used for full backups and reinstallations. Takes a full image, includes
user accounts and documents in /home folder (By default, to save space,
it excludes \~/.VirtualBox folder). - Reset accounts -- for ISOs meant
to be distributed to others. It doesn't save user account info, network
settings, and doesn't save any files in /home folder. The resulting ISO
is as close as possible to the official release with the difference that
you can customize it by updating, installing, or removing programs and
changing global settings (typically files in /etc/ folder).

By default the program starts with a GUI (program window), it can run in
terminal (CLI) if launched with -c or --cli option.

Approving the polkit prompt lets the requesting application run commands as
root through the snapshot helper for that run. If you approve a request for
another user, you are trusting both that user and their application with root access.
The prompt appears in English until the `mx-launcher-l10n` package provides
a translation for your language.

# WARNING

This tool is not actively tested on any other systems, it might or might
not work, it might or might not break things. Basically, we don't
recommend it for anything other than MX or antiX compatible systems.

# OPTIONS

  - **-h**, **--help**  
    Displays this help. Use **--help-all** to include Qt options.

  - **-v**, **--version**  
    Displays version information. Help and version options work without a graphical session.

  - **-c**, **--cli**  
    Use CLI only

  - **-d**, **--directory** \<path\>  
    Output directory

  - **-f**, **--file** \<name\>  
    Output filename. `.iso` is added if missing; when `-f` is omitted or empty,
    a default name is used. The name must not be blank or contain any of
    `< > : " / | ? *`; the GUI applies the same rules to the name field.

  - **-k**, **--kernel** \<ver, or path\>  
    Name a different kernel to use other than the default running
    kernel, use format returned by 'uname **-r**' Or the full path:
    */boot/vmlinuz-x.xx.x*...

  - **-l**, **--compression-level** \<"option"\>  
    Compression level options. Use quotes: "-Xcompression-level
    \<level\>", or "-Xalgorithm \<algorithm\>", or "-Xhc", see
    mksquashfs man page

    User preferences and this argument accept compression settings such as
    `-b 256K`, `-Xhc`, and `-Xcompression-level 15`, plus selected filesystem
    flags such as `-no-xattrs`. For unrestricted mksquashfs options, set
    `mksq_opt` in the application's root-owned, administrator-writable-only
    configuration file in `/etc`. An empty preference uses the system setting.
    If the system file is untrusted, its options must pass the same checks as
    a user preference. An unsupported preference triggers a warning and uses
    the trusted system setting, or no extra options if there is none.
    An unsupported `--compression-level` argument stops the run.
    Quote INI values containing commas, e.g.
    `mksq_opt="-Xstrategy default,fixed"`.

  - **-m**, **--month** [suffix]
    Create a monthly snapshot, add 'Month' name in the ISO name, skip
    used space calculation This option sets reset-accounts and
    compression to defaults, arguments changing those items will be
    ignored. Optionally specify a suffix to add to the month name (e.g.,
    `-m 1` for `July.1`).

  - **-n**, **--no-checksums**  
    Don't calculate checksums for resulting ISO file

  - **-p**, **--preempt**  
    Option to fix issue with calculating checksums on preempt\_rt
    kernels

  - **-r**, **--reset**  
    Resetting accounts (for distribution to others)

  - **-s**, **--checksums**  
    Calculate checksums for resulting ISO file

  - **-o**, **--override-size**  
    Skip calculating free space to see if the resulting ISO will fit

  - **--allow-omitted-mounts**
    Allow a CLI snapshot to proceed when `/usr` or `/var` is separately mounted.
    The snapshot omits the mounted contents and may be unusable. The GUI asks
    for confirmation when it detects this layout.

  - **-w**, **--workdir** \<path\>  
    Specify the path for the work directory

  - **-x**, **--exclude** \<one item\>  
    Exclude main folders, valid choices: Desktop, Documents, Downloads,
    Music, Networks, Pictures, Steam, Videos, VirtualBox. Use the option
    one time for each item you want to exclude

  - **-z**, **--compression** \<format\>  
    Compression format, valid choices: lz4, lzo, gzip, xz, zstd

# CANCELLING A RUN

Snapshot hides the terminal cursor while displaying progress and restores it
when the run ends, including after Ctrl-C or a forced exit.

SIGINT, SIGTERM and SIGHUP cancel a running snapshot in both the CLI and the
GUI: a running mksquashfs is stopped, the bind-root environment is torn down,
the log is archived, and the program exits with a failure status. A signal
that arrives before the snapshot starts, including while waiting for
authentication, exits without starting it. At the CLI exclusion-file prompt a
signal ends the program immediately, since nothing has been set up yet.

In the GUI, Next, Back and Cancel are disabled while the first authentication
request is pending, and the window cannot be closed while it is pending or
while cleanup is still tearing the snapshot down; the program exits on its own
when cleanup finishes. `--month` runs start once the window is shown.

# ARCH PACKAGING AND RELEASES

`./build.sh --arch` builds in `build/arch-package` and clears that directory
before configuration. Developer builds in `build/` are preserved, and their
cached compiler flags and test options do not affect the package.

`./release.sh` publishes the requested tag before updating the AUR recipe,
including when a previous attempt left the tag only in the local repository.
Invalid versions report their error on stderr. AUR archive directories track
`pkgver`; a leading `v` remains in the GitHub tag URL but is removed from the
package version and archive directory name. `--no-push` skips the AUR push;
it still publishes the GitHub tag and commits the AUR update.

# USER CONFIGURATION

When loading configuration, the application repairs ownership of the
`~/.config/MX-Linux` directory and its config and exclude files for the logged-in
user. System exclude lists keep their existing ownership. If a custom exclude list is missing or the default
user copy cannot be created, the application uses `/etc/<app>-exclude.list`
when available, then the packaged list under `/usr/local/share/excludes` or
`/usr/share/excludes`.

# ARCH SNAPSHOT SAFETY

Arch cleanup state is stored only in `/run/<app>/cleanup-arch.state`.
Legacy state in `/tmp` is ignored; do not move it into `/run` without verifying
its contents. Leftover mounts from an interrupted legacy run need manual cleanup.
State directories and files must be root-owned, must not be
symlinks, and must not be writable by group or other users.

The plain-bind fallback preserves the host timezone symlink and does not add
an installer shortcut to the host's `/etc/skel`. Reset snapshots still receive
the shortcut in the staged demo Desktop. Personal snapshots preserve any
existing `minstall.desktop` file or symlink on the user's Desktop.

When `/boot/archiso.img` is rebuilt, the new image must be built for the
selected kernel. An archiso preset that builds for a different kernel is
followed by a direct `mkinitcpio -k <selected kernel>` build. If that also
fails, a stale image stops the snapshot. When the image is missing, the
regular initramfs can be used if its kernel release matches the selected kernel
and `lsinitcpio` finds its `hooks/archiso` runtime hook.

# SEE ALSO

mx-live-usb-maker -- writes created ISOs to USB flashdrives
