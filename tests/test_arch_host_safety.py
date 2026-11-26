#!/usr/bin/env python3
"""Exercise Arch state and timezone safety without root or real mounts."""
import pathlib
import subprocess
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'scripts-arch/installed-to-live-arch'
FUNCTIONS = SCRIPT.read_text().rsplit('main "$@"', 1)[0]


class ArchHostSafety(unittest.TestCase):
    def run_shell(self, body):
        with tempfile.TemporaryDirectory() as directory:
            return subprocess.run(
                ['bash', '-c', FUNCTIONS + '\n' + body, 'test', directory],
                text=True, capture_output=True, check=False,
            )

    def test_legacy_state_is_ignored(self):
        result = self.run_shell('''
PRIMARY_STATE_DIR=$1/run
FALLBACK_STATE_DIR=$1/legacy
mkdir -p "$PRIMARY_STATE_DIR" "$FALLBACK_STATE_DIR"
printf 'WORK_DIR=/etc\\nFILE\\t/etc/shadow\\n' > "$FALLBACK_STATE_DIR/$STATE_FILE_NAME"
stat() { printf '0 755\\n'; }
load_state
''')
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_unsafe_state_is_rejected(self):
        for setup in (
            'ln -s "$1/other" "$PRIMARY_STATE_DIR"',
            'mkdir "$PRIMARY_STATE_DIR"; ln -s "$1/other" "$PRIMARY_STATE_DIR/$STATE_FILE_NAME"',
            'mkdir "$PRIMARY_STATE_DIR"; chmod 777 "$PRIMARY_STATE_DIR"',
            'mkdir "$PRIMARY_STATE_DIR"; touch "$PRIMARY_STATE_DIR/$STATE_FILE_NAME"; chmod 666 "$PRIMARY_STATE_DIR/$STATE_FILE_NAME"',
        ):
            with self.subTest(setup=setup):
                result = self.run_shell('''
PRIMARY_STATE_DIR=$1/run
mkdir "$1/other"
stat() { if [[ $2 == '%u %a' ]]; then printf '0 '; command stat -c %a "$3"; else command stat "$@"; fi; }
''' + setup + '\nstate_file\n')
                self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_safe_state_round_trip(self):
        result = self.run_shell('''
PRIMARY_STATE_DIR=$1/run
mkdir "$PRIMARY_STATE_DIR"
# Model root ownership while retaining the actual permission bits.
stat() { if [[ $2 == '%u %a' ]]; then printf '0 '; command stat -c %a "$3"; else command stat "$@"; fi; }
BIND_ROOT=$1/bind
WORK_DIR=$1/work
RM_FILES=("$1/created")
persist_state || exit
BIND_ROOT= WORK_DIR= RM_FILES=()
load_state || exit
[[ $BIND_ROOT == "$1/bind" && $WORK_DIR == "$1/work" && ${RM_FILES[0]} == "$1/created" ]]
''')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_start_rolls_back_only_its_own_mount_on_state_failure(self):
        for already_mounted in (False, True):
            with self.subTest(already_mounted=already_mounted):
                result = self.run_shell('''
require_root() { :; }
is_mounted() { ''' + ('true' if already_mounted else 'false') + '''; }
mount() { echo mount; }
umount() { echo umount; }
action_live_files() { echo stage; }
PRIMARY_STATE_DIR=$1/state
mkdir "$PRIMARY_STATE_DIR"
stat() { printf '0 755\\n'; }
export TMPDIR=$1
mv() { return 1; }
(main -b "$1/bind" start live-files)
rc=$?
[[ $rc != 0 && ! -e $PRIMARY_STATE_DIR/$STATE_FILE_NAME ]] || exit 1
! compgen -G "$1/$APP_NAME-bind-root-work-*" >/dev/null
''')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.splitlines(), [] if already_mounted else ['mount', 'umount'])

    def test_failed_empty_home_stops_dependent_actions(self):
        result = self.run_shell('''
do_start() { :; }
action_empty() { echo "$1"; return 1; }
action_general() { echo general; }
main -b "$1/root" start empty=/home general
''')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout.splitlines(), ['/home'])

    def test_account_removed_after_copy_failure(self):
        result = self.run_shell('''
WORK_DIR=$1/work
BIND_ROOT=$1/root
LIVE_FILES_DIR=$1/template
mkdir -p "$BIND_ROOT/etc"
touch "$BIND_ROOT/etc/passwd"
adduser() { [[ $1 == --help ]] || printf '%s' "${!#}" > "$WORK_DIR/added"; }
useradd() { adduser "$@"; }
deluser() { printf '%s' "$1" > "$WORK_DIR/removed"; echo removed; echo diagnostic >&2; }
userdel() { deluser "$@"; }
findmnt() { echo ext4; }
cp() { return 1; }
action_passwd && exit 1
[[ -s $WORK_DIR/added ]] && cmp "$WORK_DIR/added" "$WORK_DIR/removed"
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '')
        self.assertIn('diagnostic', result.stderr)

    def test_failed_account_rewrite_never_reaches_bind_mount(self):
        for failure in ('ln -s /dev/full "$WORK_DIR/bind/etc/passwd.new"',
                        'chmod() { return 1; }', 'chown() { return 1; }',
                        'mv() { return 1; }'):
            with self.subTest(failure=failure):
                result = self.run_shell('''
WORK_DIR=$1/work
BIND_ROOT=$1/root
REAL_ROOT=$BIND_ROOT
LIVE_FILES_DIR=$1/template
mkdir -p "$REAL_ROOT/etc" "$WORK_DIR/bind/etc"
echo 'root:x:0:0::/root:/bin/sh' > "$REAL_ROOT/etc/passwd"
adduser() { :; }; useradd() { :; }; deluser() { :; }; userdel() { :; }
findmnt() { echo ext4; }
bind_mount() { echo mount; }
''' + failure + '\naction_passwd\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, '')

    def test_reset_selects_home_accounts_in_configured_uid_range(self):
        result = self.run_shell('''
WORK_DIR=$1/work
BIND_ROOT=$1/root
REAL_ROOT=$BIND_ROOT
LIVE_FILES_DIR=$1/template
mkdir -p "$REAL_ROOT/etc" "$BIND_ROOT/home/demo"
printf 'UID_MIN 2000\nUID_MAX 3000\n' > "$REAL_ROOT/etc/login.defs"
cat > "$REAL_ROOT/etc/passwd" <<'USERS'
low:x:1999:1::/home/low:/bin/sh
nu:x:2000:1::/home/nu:/usr/bin/nu
upper:x:3000:1::/home/upper:/bin/bash
high:x:3001:1::/home/high:/bin/sh
service:x:2200:1::/var/lib/service:/bin/sh
USERS
printf '%s' 'last:x:2400:1::/home/last:/bin/nu' >> "$REAL_ROOT/etc/passwd"
printf '%s' 'staff:x:2001:service,last' > "$REAL_ROOT/etc/group"
printf '%s' 'service:100000:65536' > "$REAL_ROOT/etc/subuid"
adduser() { :; }
useradd() { :; }
deluser() { :; }
userdel() { :; }
bind_mount() { :; }
findmnt() { echo ext4; }
action_passwd || exit 1
cut -d: -f1 "$WORK_DIR/bind/etc/passwd"
grep -qx 'staff:x:2001:service' "$WORK_DIR/bind/etc/group" &&
    grep -qx 'service:100000:65536' "$WORK_DIR/bind/etc/subuid"
''')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), ['low', 'high', 'service'])

    def test_reset_demo_has_one_matching_group_and_gshadow_entry(self):
        for temp_gid in (1000, 1001):
            with self.subTest(temp_gid=temp_gid):
                result = self.run_shell('''
WORK_DIR=$1/work
BIND_ROOT=$1/root
REAL_ROOT=$BIND_ROOT
LIVE_FILES_DIR=$1/template
mkdir -p "$BIND_ROOT/etc" "$BIND_ROOT/home/demo"
echo 'alice:x:1000:100::/home/alice:/bin/sh' > "$BIND_ROOT/etc/passwd"
echo 'users:x:100:' > "$BIND_ROOT/etc/group"
echo 'users:!::' > "$BIND_ROOT/etc/gshadow"
temp_gid=''' + str(temp_gid) + '''
useradd() {
    local name=${!#}
    echo "$name:x:1001:$temp_gid::/home/$name:/bin/sh" >> "$BIND_ROOT/etc/passwd"
    echo "$name:x:$temp_gid:" >> "$BIND_ROOT/etc/group"
    echo "$name:!::" >> "$BIND_ROOT/etc/gshadow"
}
adduser() { [[ $1 == --help ]] || useradd "$@"; }
deluser() { :; }; userdel() { :; }; bind_mount() { :; }; findmnt() { echo ext4; }
action_passwd || exit
cd "$WORK_DIR/bind/etc" || exit
cat passwd group gshadow
''')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertCountEqual(result.stdout.splitlines(), [
                    'demo:x:1000:1000::/home/demo:/bin/sh',
                    'users:x:100:', 'demo:x:1000:', 'users:!::', 'demo:!::',
                ])

    def test_failed_cleanup_record_prevents_host_changes(self):
        for kind in ('directory', 'file', 'symlink'):
            with self.subTest(kind=kind):
                result = self.run_shell('''
REAL_ROOT=$1/host
BIND_ROOT=$REAL_ROOT
mkdir -p "$REAL_ROOT/etc" "$1/template/etc"
ln -s /dev/null "$1/template/etc/test"
persist_state() { return 1; }
bind_root_is_isolated() { return 1; }
target=$REAL_ROOT/etc/test
(case ''' + kind + ''' in
    directory) make_dir "$target" "$target" ;;
    file) touch_file "$target" "$target" ;;
    symlink) bind_mount_template "$1/template" ;;
esac) && exit 1
[[ ! -e $target && ! -L $target ]]
''')
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_service_mask_stays_in_overlay(self):
        result = self.run_shell('''
REAL_ROOT=$1/host
BIND_ROOT=$1/bind
mkdir -p "$REAL_ROOT/etc" "$BIND_ROOT/etc" "$1/template/etc"
ln -s /dev/null "$1/template/etc/test.service"
findmnt() { echo overlay; }
stat() { if [[ $3 == "$BIND_ROOT" ]]; then echo 2; else echo 1; fi; }
bind_mount_template "$1/template" || exit
[[ ! -L $REAL_ROOT/etc/test.service && $(readlink "$BIND_ROOT/etc/test.service") == /dev/null
   && ${#RM_FILES[@]} == 0 ]]
''')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_timezone_preserves_plain_bind_even_on_overlay_host(self):
        for filesystem, distinct_device, replaced in (
            ('ext4', False, False), ('overlay', False, False), ('overlay', True, True),
        ):
            with self.subTest(filesystem=filesystem, distinct_device=distinct_device):
                result = self.run_shell('''
BIND_ROOT=$1/bind
REAL_ROOT=$1/host
WORK_DIR=$1/work
zone_file=$REAL_ROOT/usr/share/zoneinfo/Etc/UTC
mkdir -p "$BIND_ROOT/etc" "${zone_file%/*}" "$WORK_DIR"
printf 'test zone data' > "$zone_file"
ln -s "$zone_file" "$BIND_ROOT/etc/localtime"
# All writes are confined to the test tree; mount operations are mocked.
bind_mount_template() { [[ -f $1/etc/localtime && -f $1/etc/timezone ]]; }
findmnt() { printf '%s\\n' "''' + filesystem + '''"; }
stat() { if [[ $3 == "$BIND_ROOT" ]]; then echo ''' + ('2' if distinct_device else '1') + '''; else echo 1; fi; }
stage_timezone Etc/UTC "$zone_file" || exit
''' + ('[[ -f $BIND_ROOT/etc/localtime && ! -L $BIND_ROOT/etc/localtime ]]' if replaced
       else '[[ -L $BIND_ROOT/etc/localtime ]]') + '\n')
                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
