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
mkdir -p "$BIND_ROOT/etc" "$REAL_ROOT" "$WORK_DIR"
ln -s /usr/share/zoneinfo/UTC "$BIND_ROOT/etc/localtime"
# All writes are confined to the test tree; mount operations are mocked.
bind_mount_template() { [[ -f $1/etc/localtime && -f $1/etc/timezone ]]; }
findmnt() { printf '%s\\n' "''' + filesystem + '''"; }
stat() { if [[ $3 == "$BIND_ROOT" ]]; then echo ''' + ('2' if distinct_device else '1') + '''; else echo 1; fi; }
action_timezone || exit
''' + ('[[ -f $BIND_ROOT/etc/localtime && ! -L $BIND_ROOT/etc/localtime ]]' if replaced
       else '[[ -L $BIND_ROOT/etc/localtime ]]') + '\n')
                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
