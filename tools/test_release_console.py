"""Destructive cases run only in temporary fixture repositories, never real mod folders."""
from pathlib import Path
import contextlib
import io
import json
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from release_console import Console, SPECS, Blocked, digest, write_json, read_json
import package_guard


def run_git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        addons = self.base / 'addons'
        for mod, spec in SPECS.items():
            root = addons / spec['folder']
            root.mkdir(parents=True)
            run_git(root, 'init', '-q')
            run_git(root, 'config', 'user.email', 'fixture@example.invalid')
            run_git(root, 'config', 'user.name', 'Fixture')
            (root / '.gitignore').write_text('.release-console/\nresourceDatabase.rdb\n')
            for name in spec['files'] + ['resourceDatabase.rdb']:
                (root / name).write_text(name)
            (root / 'CHANGELOG.md').write_text('# Changelog\n\n## 0.1.0\n\n- Reviewed fixture release.\n')
            deps = f'"{SPECS["BBB"]["guid"]}"' if mod == 'ACA' else ''
            (root / 'addon.gproj').write_text(f'GameProject {{\n GUID "{spec["guid"]}"\n TITLE "Development"\n Dependencies {{ {deps} }}\n}}')
            for directory in spec['dirs']:
                target = root / directory / 'fixture.c'
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text('class Fixture {}\n')
            tools = root / 'tools'
            tools.mkdir()
            (tools / 'check_mission_doc.py').write_text('print("fixture docs pass")')
            run_git(root, 'add', '.')
            run_git(root, 'commit', '-qm', 'fixture')
        self.root = addons / SPECS['ACA']['folder']
        self.console = Console(self.root)
        self.bbb = self.console.source('BBB')
        write_json(self.root / 'tools/bbb-source.json', {'commit': run_git(self.bbb, 'rev-parse', 'HEAD')})
        self.commit(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def commit(self, root):
        run_git(root, 'add', '.')
        run_git(root, 'commit', '-qm', 'update')

    def mark_checked(self, mod):
        parent = self.console.stage(mod)
        write_json(parent / 'STARTUP-CHECK.json', {'manifest': digest(parent / 'WORKSHOP-BUILD.json')})
        write_json(self.console.pointer(mod), {'candidate': str(parent)})
        return parent

    def test_bbb_runtime_tree_ships_without_test_fixtures(self):
        runtime = self.bbb / 'Prefabs/BBB/FireHandheldFlare.bt'
        runtime.parent.mkdir(parents=True, exist_ok=True)
        runtime.write_text('BehaviorTree { Name "handheld flare fixture" }\n')
        meta = Path(str(runtime) + '.meta')
        meta.write_text('MetaFileClass { Name "{BBB0F1A200000001}Prefabs/BBB/FireHandheldFlare.bt" }\n')
        layout = self.bbb / 'UI/layouts/BBB/BBB_SettingsTab.layout'
        layout.parent.mkdir(parents=True, exist_ok=True)
        layout.write_text('HorizontalLayoutWidgetClass { Name "settings tab fixture" }\n')
        test_script = self.bbb / 'Scripts/Game/BBB_Tests/BBB_FlareTest.c'
        test_script.parent.mkdir(parents=True, exist_ok=True)
        test_script.write_text('class BBB_FlareTest {}\n')
        test_prefab = self.bbb / 'Prefabs/BBB_Tests/FlareFixture.et'
        test_prefab.parent.mkdir(parents=True, exist_ok=True)
        test_prefab.write_text('GenericEntity {}\n')
        changelog = self.bbb / 'CHANGELOG.md'
        changelog.write_text(changelog.read_text() + '- Reviewed flare fixture.\n')
        self.commit(self.bbb)
        parent = self.console.stage('BBB')
        payload = parent / SPECS['BBB']['folder']
        self.assertEqual((payload / runtime.relative_to(self.bbb)).read_bytes(), runtime.read_bytes())
        self.assertEqual((payload / meta.relative_to(self.bbb)).read_bytes(), meta.read_bytes())
        self.assertEqual((payload / layout.relative_to(self.bbb)).read_bytes(), layout.read_bytes())
        self.assertFalse((payload / test_script.relative_to(self.bbb)).exists())
        self.assertFalse((payload / test_prefab.relative_to(self.bbb)).exists())

    def test_fresh_candidate_and_unique_rebuild(self):
        first = self.mark_checked('BBB')
        self.assertEqual(first, self.console.checked('BBB'))
        second = self.console.stage('BBB')
        self.assertNotEqual(first, second)
        self.assertTrue(first.is_dir())

    def test_publication_records_evidence_and_opens_next_version(self):
        parent = self.mark_checked('BBB')
        result = self.console.record_publication('BBB', '0.1.0', 'Owner confirmed', parent)
        entry = result['releases'][0]
        self.assertEqual(entry['sourceCommit'], run_git(self.bbb, 'rev-parse', 'HEAD'))
        text = (self.bbb / 'CHANGELOG.md').read_text(encoding='utf-8')
        self.assertIn('## 0.1.1 — unreleased', text)
        self.assertIn('## 0.1.0 — released', text)
        self.assertEqual(result, self.console.record_publication('BBB', '0.1.0', 'Repeated confirmation'))
        with self.assertRaisesRegex(Blocked, 'already published'):
            self.console.require_new_version('BBB', '0.1.0')
        self.console.require_new_version('BBB', '0.1.1')

    def test_published_notes_are_locked_and_empty_next_version_cannot_ship(self):
        self.console.record_publication('BBB', '0.1.0', 'Owner confirmed')
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'reviewed release bullets'):
            self.console.stage('BBB')
        # ACA may still validate the unchanged, already-published dependency.
        self.console.stage('BBB', dependency_only=True)
        path = self.bbb / 'CHANGELOG.md'
        path.write_text(path.read_text(encoding='utf-8').replace('Reviewed fixture release.', 'New gameplay under old version.'), encoding='utf-8')
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'Published release notes changed'):
            self.console.stage('BBB')

    def test_recording_older_release_preserves_existing_next_notes(self):
        path = self.bbb / 'CHANGELOG.md'
        path.write_text('# Changelog\n\n## 0.1.1 — unreleased\n\n- New behavior.\n\n## 0.1.0\n\n- Old behavior.\n', encoding='utf-8')
        self.console.record_publication('BBB', '0.1.0', 'Owner confirmed')
        self.assertTrue(path.read_text(encoding='utf-8').startswith('# Changelog\n\n## 0.1.1'))
        self.assertIn('- New behavior.', path.read_text(encoding='utf-8'))
        self.commit(self.bbb)
        self.console.stage('BBB')

    def test_publication_candidate_version_mismatch_does_not_write_history(self):
        parent = self.mark_checked('BBB')
        with self.assertRaisesRegex(Blocked, 'candidate/version mismatch'):
            self.console.record_publication('AIO', '0.1.0', 'Owner confirmed', parent)
        self.assertFalse((self.console.source('AIO') / 'WORKSHOP-RELEASES.json').exists())

    def test_publication_cannot_attribute_changed_notes_to_uploaded_candidate(self):
        parent = self.mark_checked('BBB')
        path = self.bbb / 'CHANGELOG.md'
        path.write_text(path.read_text().replace('Reviewed fixture release.', 'Changed after upload.'))
        with self.assertRaisesRegex(Blocked, 'Source notes differ'):
            self.console.record_publication('BBB', '0.1.0', 'Upload confirmed', parent)
        self.assertFalse((self.bbb / 'WORKSHOP-RELEASES.json').exists())

    def test_dirty_source_refused(self):
        (self.bbb / 'README.md').write_text('unsaved')
        with self.assertRaisesRegex(Blocked, 'unsaved source'):
            self.console.stage('BBB')

    def test_source_commit_advance_refused(self):
        self.mark_checked('BBB')
        (self.bbb / 'README.md').write_text('next')
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'moved on'):
            self.console.checked('BBB')

    def test_candidate_edit_refused(self):
        parent = self.mark_checked('BBB')
        (parent / 'BetterBattleBuddies/README.md').write_text('edited release')
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_extra_and_missing_candidate_files_refused(self):
        parent = self.mark_checked('BBB')
        extra = parent / 'BetterBattleBuddies/stale.c'
        extra.write_text('stale')
        with self.assertRaisesRegex(Blocked, 'file set changed'):
            self.console.checked('BBB')
        extra.unlink()
        (parent / 'BetterBattleBuddies/README.md').unlink()
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_no_startup_receipt_refused(self):
        parent = self.console.stage('BBB')
        write_json(self.console.pointer('BBB'), {'candidate': str(parent)})
        with self.assertRaises(FileNotFoundError):
            self.console.checked('BBB')

    def test_failed_startup_never_promotes_candidate(self):
        parent = self.console.stage('BBB')
        with patch('release_console.subprocess.run', side_effect=subprocess.CalledProcessError(1, 'startup')):
            with self.assertRaises(subprocess.CalledProcessError):
                self.console.smoke(parent)
        self.assertFalse(self.console.pointer('BBB').exists())

    def test_wrong_mod_pointer_refused(self):
        parent = self.mark_checked('AIO')
        write_json(self.console.pointer('BBB'), {'candidate': str(parent)})
        with self.assertRaisesRegex(Blocked, 'different mod'):
            self.console.checked('BBB')

    def test_changed_manifest_invalidates_startup(self):
        parent = self.mark_checked('BBB')
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        manifest['builtUtc'] = 'changed'
        write_json(parent / 'WORKSHOP-BUILD.json', manifest)
        with self.assertRaisesRegex(Blocked, 'Startup check'):
            self.console.checked('BBB')

    def test_ignored_resource_database_change_refused(self):
        self.mark_checked('BBB')
        (self.bbb / 'resourceDatabase.rdb').write_text('rebuilt')
        with self.assertRaisesRegex(Blocked, 'source content changed'):
            self.console.checked('BBB')

    def test_workbench_regenerated_candidate_index_allowed_but_missing_refused(self):
        parent = self.mark_checked('BBB')
        index = parent / 'BetterBattleBuddies/resourceDatabase.rdb'
        index.write_bytes(b'Workbench regenerated resource index')
        self.assertEqual(parent, self.console.checked('BBB'))
        index.unlink()
        with self.assertRaisesRegex(Blocked, 'edited or damaged'):
            self.console.checked('BBB')

    def test_output_inside_addons_refused(self):
        with self.assertRaisesRegex(Blocked, 'OUTSIDE'):
            Console(self.root, self.root.parent / '_bad-release')

    def test_wrong_bbb_pin_refused(self):
        write_json(self.root / 'tools/bbb-source.json', {'commit': '0' * 40})
        self.commit(self.root)
        with self.assertRaisesRegex(Blocked, 'pinned dependency'):
            self.console.stage('ACA')

    def test_release_notes_generated_and_history_preserved(self):
        notes = '# Changelog\n\n## 0.2.0 — next\n\n- New player feature.\n\n## 0.1.0\n\n- Old release.\n'
        (self.bbb / 'CHANGELOG.md').write_text(notes, encoding='utf-8')
        self.commit(self.bbb)
        parent = self.mark_checked('BBB')
        info = self.console.notes_info('BBB')
        self.assertEqual('0.2.0', info['version'])
        output = Path(info['changelogFile']).read_text(encoding='utf-8')
        self.assertIn('New player feature.', output)
        self.assertNotIn('Old release.', output)
        self.assertIn('Old release.', (parent / 'CHANGELOG.md').read_text(encoding='utf-8'))

    def test_release_notes_tamper_refused(self):
        parent = self.mark_checked('BBB')
        (parent / 'WORKSHOP-CHANGELOG.txt').write_text('incorrect notes')
        with self.assertRaisesRegex(Blocked, 'release notes changed'):
            self.console.checked('BBB')

    def test_missing_unversioned_and_placeholder_notes_refused(self):
        path = self.bbb / 'CHANGELOG.md'
        for value in ['# Changelog', '## Unreleased\n- Feature', '## 0.1.0\n- TODO', '## 0.1.0\nEmpty']:
            path.write_text(value)
            self.commit(self.bbb)
            with self.assertRaises(Blocked):
                self.console.stage('BBB')
        path.unlink()
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'CHANGELOG.md'):
            self.console.stage('BBB')

    def test_production_after_notes_review_requires_changelog_update(self):
        path = self.bbb / 'Scripts/Game/BBB/fixture.c'
        path.write_text('class NewFeature {}')
        self.commit(self.bbb)
        with self.assertRaisesRegex(Blocked, 'production changed'):
            self.console.stage('BBB')
        with (self.bbb / 'CHANGELOG.md').open('a') as handle:
            handle.write('- Added the new feature.\n')
        self.commit(self.bbb)
        self.console.stage('BBB')

    def test_cli_publish_refuses_current_and_older_versions(self):
        self.mark_checked('BBB')
        for version in ['0.1.0', '0.2.0']:
            with patch.object(self.console, 'downloaded_version', return_value=version):
                with self.assertRaisesRegex(Blocked, 'higher version'):
                    self.console.publish_info('BBB', self.base)
        with patch.object(self.console, 'downloaded_version', return_value='0.0.9'):
            info = self.console.publish_info('BBB', self.base)
            self.assertEqual('0.1.0', info['version'])
            self.assertTrue(Path(info['changelogFile']).is_file())

    def test_installed_version_read_from_game_metadata_and_manifest_fallback(self):
        installed = self.base / 'downloaded'
        copy = installed / 'BBB'
        copy.mkdir(parents=True)
        (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        data = copy / 'ServerData.json'
        data.write_text('{"revision":{"version":"0.1.1"}, trailing incomplete JSON')
        self.assertEqual('0.1.1', self.console.downloaded_version('BBB', installed))
        data.unlink()
        (copy / 'addon.gproj_0.1.2_manifest.json').write_text('{}')
        self.assertEqual('0.1.2', self.console.downloaded_version('BBB', installed))

    def test_aca_test_scripts_excluded(self):
        test = self.root / 'Scripts/Game/ACR_Tests/test.c'
        test.parent.mkdir(parents=True)
        test.write_text('test')
        self.commit(self.root)
        parent = self.console.stage('ACA')
        self.assertFalse((parent / 'ArmaCommanderReforger/Scripts/Game/ACR_Tests').exists())

    def test_installed_missing_and_stale_blocked(self):
        self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        installed.mkdir()
        with self.assertRaisesRegex(Blocked, 'downloaded Workshop copy'):
            self.console.installed('BBB', installed)
        copy = installed / 'BBB'
        copy.mkdir()
        (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        with self.assertRaisesRegex(Blocked, 'file set differs'):
            self.console.installed('BBB', installed)

    def test_installed_content_match_crlf_and_stale_bytes(self):
        parent = self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        copy = installed / 'BBB'
        copy.mkdir(parents=True)
        (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        manifest = read_json(parent / 'WORKSHOP-BUILD.json')
        index = {}
        payload = bytearray()
        pak = copy / 'data.pak'
        for item in manifest['files']:
            if item['path'].startswith(('Scripts/', 'Configs/')) and item['path'].endswith(('.c', '.conf')):
                data = (Path(manifest['addonRoot']) / item['path']).read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                index[item['path']] = [str(pak), len(payload), len(data), len(data), 0]
                payload.extend(data)
        pak.write_bytes(payload)
        def fake_index(directory, path):
            write_json(Path(path), index)
            print('index progress must not contaminate the project path')
        with patch('pakread.index', side_effect=fake_index):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.console.installed('BBB', installed)
            self.assertEqual('', output.getvalue())
            pak.write_bytes(b'X' + payload[1:])
            with self.assertRaisesRegex(Blocked, 'content is stale'):
                self.console.installed('BBB', installed)

    def test_duplicate_installed_mod_refused(self):
        self.mark_checked('BBB')
        installed = self.base / 'downloaded'
        for name in ['copy1', 'copy2']:
            copy = installed / name
            copy.mkdir(parents=True)
            (copy / 'addon.gproj').write_text(f'GUID "{SPECS["BBB"]["guid"]}"')
        with self.assertRaisesRegex(Blocked, 'found 2'):
            self.console.installed('BBB', installed)

    def test_aca_receipt_binds_exact_bbb_candidate(self):
        bbb = self.mark_checked('BBB')
        aca = self.mark_checked('ACA')
        receipt = read_json(aca / 'STARTUP-CHECK.json')
        receipt['dependencyManifest'] = digest(bbb / 'WORKSHOP-BUILD.json')
        write_json(aca / 'STARTUP-CHECK.json', receipt)
        self.assertEqual(aca, self.console.checked('ACA'))
        self.mark_checked('BBB')
        with self.assertRaisesRegex(Blocked, 'different BBB candidate'):
            self.console.checked('ACA')

    def test_package_provenance_rejects_replaced_zip(self):
        dist = self.root / 'dist'
        dist.mkdir()
        # Keep fixture output ignored, as it is in the real repository.
        with (self.root / '.gitignore').open('a') as handle:
            handle.write('dist/\n')
        self.commit(self.root)
        archive = dist / 'ACActual-WithBBB.zip'
        archive.write_bytes(b'original')
        (dist / 'VERSION.txt').write_text('fixture')
        sha = run_git(self.bbb, 'rev-parse', 'HEAD')
        write_json(dist / 'PACKAGE-BUILD.json', dict(commit=run_git(self.root, 'rev-parse', 'HEAD'),
                   bbbCommit=sha, version='fixture', sha256=digest(archive)))
        with patch.object(package_guard, 'bbb_source', return_value=(self.bbb, sha)):
            package_guard.verify(self.root)
            archive.write_bytes(b'wrong archive')
            with self.assertRaisesRegex(Blocked, 'replaced or edited'):
                package_guard.verify(self.root)


if __name__ == '__main__':
    unittest.main()
