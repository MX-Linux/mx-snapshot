#!/usr/bin/env python3
"""Release regression checks using disposable repositories and no network."""
import os
from pathlib import Path
import shutil
import secrets
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseWorkflows(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.env = dict(os.environ, GIT_CONFIG_GLOBAL='/dev/null', GIT_CONFIG_NOSYSTEM='1', GIT_AUTHOR_NAME='Release test', GIT_AUTHOR_EMAIL='test@example.invalid',
                        GIT_COMMITTER_NAME='Release test', GIT_COMMITTER_EMAIL='test@example.invalid')
        self.repo = self.directory / 'project'
        self.run_command(['git', 'init', '-q', '-b', 'main', str(self.repo)])
        (self.repo / 'file').write_text('test\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'Initial')

    def run_command(self, command, cwd=None, check=True, input_text=None):
        return subprocess.run(command, cwd=cwd, env=self.env, input=input_text, text=True, capture_output=True, check=check)

    def git(self, *args):
        return self.run_command(['git', '-C', str(self.repo), *args]).stdout.strip()

    def test_invalid_version_has_visible_error(self):
        result = self.run_command(['bash', str(ROOT / 'release.sh'), '26.10-1'], self.repo, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Invalid version format', result.stderr)
        self.assertIn('Expected formats', result.stderr)

    def test_latest_tag_compares_versions_and_keeps_the_original_spelling(self):
        self.git('remote', 'add', 'origin', str(self.repo))
        self.git('switch', '-qc', 'test-release')
        self.env['MAIN_BRANCH'] = 'main'
        self.git('tag', 'v26.9')
        for latest in ('26.11', 'v26.12'):
            self.git('tag', latest)
            # Decline the release when prompted about the current branch.
            result = self.run_command(['bash', str(ROOT / 'release.sh')], self.repo, input_text='n\n')
            self.assertIn(f"using latest tag '{latest}'", result.stdout)

    @unittest.skipUnless(shutil.which('makepkg'), 'makepkg needed for metadata checks')
    @unittest.skipIf(os.geteuid() == 0, 'makepkg refuses to run as root')
    def test_local_tag_push_recovery_and_archive_directory(self):
        remote = self.directory / 'remote.git'
        self.run_command(['git', 'init', '-q', '--bare', str(remote)])
        self.git('remote', 'add', 'mxlinux', str(remote))
        pkgver = f'99.98.{secrets.randbits(64)}g'
        version = 'v' + pkgver
        self.git('tag', '-a', version, '-m', 'Regression release')
        aur = self.repo / 'aur'
        self.run_command(['git', 'init', '-q', '-b', 'main', str(aur)])
        (aur / 'PKGBUILD').write_text('''pkgname=mx-snapshot
pkgver=26.10
pkgrel=1
pkgdesc='test'
arch=('x86_64')
url='https://mxlinux.org'
license=('GPL3')
source=('https://github.com/MX-Linux/mx-snapshot/archive/refs/tags/26.10.tar.gz')
sha256sums=('SKIP')
_srcdir="mx-snapshot-26.10"
''')
        self.run_command(['git', '-C', str(aur), 'add', '.'])
        self.run_command(['git', '-C', str(aur), 'commit', '-qm', 'Initial recipe'])
        (aur / 'staged-note').write_text('keep staged')
        self.run_command(['git', 'add', 'staged-note'], aur)
        (aur / 'untracked-note').write_text('keep untracked')
        tools = self.directory / 'bin'
        tools.mkdir()
        curl = tools / 'curl'
        curl.write_text('#!/bin/bash\nwhile [[ $# -gt 0 ]]; do if [[ $1 == -o ]]; then printf test > "$2"; exit; fi; shift; done\nexit 1\n')
        curl.chmod(0o755)
        self.env['PATH'] = str(tools) + os.pathsep + self.env['PATH']
        for flag in ('--update', '--force'):
            result = self.run_command(['bash', str(ROOT / 'release.sh'), flag, '--no-push', version],
                                      self.repo, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(self.git('ls-remote', '--tags', 'mxlinux'), '')
            self.assertEqual(self.run_command(['git', 'rev-list', '--count', 'HEAD'], aur).stdout.strip(), '1')

        for _ in range(2):
            self.run_command(['bash', str(ROOT / 'release.sh'), '--no-push', version], self.repo)
            self.assertEqual(self.run_command(['git', 'rev-list', '--count', 'HEAD'], aur).stdout.strip(), '2')
            self.assertEqual(self.run_command(['git', 'diff', '--name-only', 'HEAD^', 'HEAD'], aur).stdout.splitlines(),
                             ['.SRCINFO', 'PKGBUILD'])
            self.assertEqual(self.run_command(['git', 'status', '--porcelain'], aur).stdout.splitlines(),
                             ['A  staged-note', '?? untracked-note'])
            remote_tag = self.run_command(['git', '--git-dir', str(remote), 'rev-parse', 'refs/tags/' + version]).stdout.strip()
            self.assertEqual(remote_tag, self.git('rev-parse', 'refs/tags/' + version))
            metadata = self.run_command(['makepkg', '--printsrcinfo'], aur).stdout
            self.assertIn('pkgver = ' + pkgver, metadata)
            resolved = self.run_command(['bash', '-c', 'source PKGBUILD; printf "%s" "$_srcdir"'], aur).stdout
            self.assertEqual(resolved, 'mx-snapshot-' + pkgver)

        # A recipe-only retry selects the published tag even with a newer local tag.
        self.git('tag', '-d', version)
        self.git('tag', '999.1')
        self.run_command(['bash', str(ROOT / 'release.sh'), '--update', '--no-push'], self.repo)
        self.assertEqual(self.git('tag', '-l'), '999.1')
        self.assertEqual(self.git('ls-remote', '--tags', '--refs', 'mxlinux').split()[1], 'refs/tags/' + version)
        self.assertEqual(self.run_command(['git', 'rev-list', '--count', 'HEAD'], aur).stdout.strip(), '2')

    def test_remote_failure_aborts_before_aur_edits(self):
        self.git('tag', '99.97g')
        self.git('remote', 'add', 'mxlinux', str(self.directory / 'missing.git'))
        result = self.run_command(['bash', str(ROOT / 'release.sh'), '--no-push', '99.97g'], self.repo, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Could not check tag', result.stderr)
        self.assertFalse((self.repo / 'aur').exists())

    def test_clean_preserves_legacy_and_current_arch_packages(self):
        script = self.repo / 'build.sh'
        shutil.copy2(ROOT / 'build.sh', script)
        (self.repo / 'CMakeLists.txt').touch()
        build = self.repo / 'build'
        build.mkdir()
        legacy = build / 'example-1.0-1-x86_64.pkg.tar.zst'
        signature = build / (legacy.name + '.sig')
        legacy.write_bytes(b'old package')
        signature.write_bytes(b'signature')
        for with_arch_dir in (False, True):
            with self.subTest(with_arch_dir=with_arch_dir):
                if with_arch_dir:
                    (build / 'arch').mkdir()
                    (build / 'arch' / legacy.name).write_bytes(b'new package')
                (build / '.ninja_log').touch()
                self.run_command([str(script), 'clean'], self.repo)
                self.assertEqual(legacy.read_bytes(), b'old package')
                self.assertEqual(signature.read_bytes(), b'signature')
                self.assertFalse((build / '.ninja_log').exists())
                if with_arch_dir:
                    self.assertEqual((build / 'arch' / legacy.name).read_bytes(), b'new package')

    def test_package_build_keeps_developer_cache_and_discards_package_cache(self):
        build = self.repo / 'build'
        package_build = build / 'arch-package'
        package_build.mkdir(parents=True)
        (build / 'CMakeCache.txt').write_text('developer ASAN cache')
        (package_build / 'CMakeCache.txt').write_text('stale package cache')
        self.env['PKGVER'] = '99.12.3'
        command = r'''source "$1"
startdir=$2
cmake() {
    [[ -f build/CMakeCache.txt ]] || return 1
    [[ ! -f build/arch-package/CMakeCache.txt ]] || return 2
    if [[ $1 != --build ]]; then
        printf '%s\n' "$@" > configure-args
        while [[ $# -gt 0 ]]; do
            if [[ $1 == -B ]]; then [[ $2 == build/arch-package ]] || return 3; break; fi
            shift
        done
    else
        [[ $2 == build/arch-package ]] || return 4
    fi
}
build
'''
        self.run_command(['bash', '-e', '-c', command, 'test', str(ROOT / 'PKGBUILD'), str(self.repo)])
        self.assertIn('-DGIT_TAG_VERSION=99.12.3', (self.repo / 'configure-args').read_text().splitlines())
        self.assertEqual((build / 'CMakeCache.txt').read_text(), 'developer ASAN cache')
        self.assertFalse((package_build / 'CMakeCache.txt').exists())


if __name__ == '__main__':
    unittest.main()
