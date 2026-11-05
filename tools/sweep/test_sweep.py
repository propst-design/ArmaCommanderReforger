"""Unit tests for the sweep's result parsing (no game needed): python -m unittest discover tools/sweep"""
import sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sweep, playlog  # noqa: E402


def result(tmp, name, text):
    p = Path(tmp) / '.acr-requests' / name
    p.parent.mkdir(exist_ok=True)
    p.write_text(text, encoding='utf-8')
    return p


class Judge(unittest.TestCase):
    def test_verdict_pass_needs_completed(self):
        self.assertEqual(sweep.judge('completed', '[ACR] VERDICT PASS test=x passed=5 failed=0 expected=5', 'srv:x')[0], 'PASS')
        self.assertEqual(sweep.judge('server exited', '[ACR] VERDICT PASS test=x passed=5', 'srv:x')[0], 'ERROR')
        self.assertEqual(sweep.judge('timeout', '', 'srv:x')[0], 'TIMEOUT')
        self.assertEqual(sweep.judge('refused', '', 'srv:x')[0], 'REFUSED')

    def test_any_fail_line_fails(self):
        text = '[ACR] VERDICT PASS test=a passed=2 failed=0 expected=2\n[ACR] VERDICT FAIL test=a role=client passed=1 failed=1 expected=2'
        v, passed, failed, *_ = sweep.judge('completed', text, 'srv:a')
        self.assertEqual((v, passed, failed), ('FAIL', 3, 1))

    def test_no_expected_count_is_undeclared(self):
        self.assertEqual(sweep.judge('completed', '[ACR] VERDICT PASS test=x passed=3 failed=0', 'srv:x')[0], 'PASS-UNDECLARED')

    def test_bbb_failed_status_is_a_test_failure(self):
        self.assertEqual(sweep.judge('failed', '[ACR] VERDICT FAIL test=b passed=1 failed=2', 'b')[0], 'FAIL')
        self.assertEqual(sweep.judge('failed', '', 'b')[0], 'FAIL')

    def test_aio_and_mcc_summaries(self):
        self.assertEqual(sweep.judge('completed', '[AIO] TEST RESULT 7 passed, 0 failed', 'tasks')[:3], ('PASS-UNDECLARED', 7, 0))
        self.assertEqual(sweep.judge('completed', '[AIO] TEST RESULT 6 passed, 1 failed', 'tasks')[0], 'FAIL')
        self.assertEqual(sweep.judge('done', '[MCC] TEST DONE 27 checks, 0 failed', 'zones')[:3], ('PASS-UNDECLARED', 27, 0))

    def test_boots_and_legacy(self):
        self.assertEqual(sweep.judge('completed', '', 'pak:compile-aca')[0], 'PASS')
        self.assertEqual(sweep.judge('completed', '', 'matrix:ours')[0], 'PASS')
        self.assertEqual(sweep.judge('completed', '[ACR] TEST COMPLETE', 'srv:old')[0], 'NOVERDICT')


class Parse(unittest.TestCase):
    def test_aca_result_with_build_label(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = result(tmp, 'x1.result.txt', '﻿status=completed\npreset=srv:regress\nstarted=2026-11-04T18:37:14\nseconds=75\n---\n'
                       '18:37:17 SCRIPT    (E): Virtual Machine Exception\nReason: NULL pointer\nClass: \'SCR_AIProcessFailedMovementResult\'\n'
                       '18:38:00 SCRIPT    (E): ACR_Foo broke\n[ACR] VERDICT PASS test=regress passed=142 failed=0 expected=142\n')
            p.with_name('x1.build.txt').write_text('4c94098 bbb 0369a03')
            r = sweep.parse_result(p)
            self.assertEqual((r['repo'], r['preset'], r['mode'], r['build'], r['bbb']), ('ACA', 'regress', 'srv', '4c94098', '0369a03'))
            self.assertEqual((r['verdict'], r['passed'], r['seconds']), ('PASS', 142, '75'))
            self.assertEqual(r['errors'], 1)   # the vanilla null-leader exception is noise


class PlayLog(unittest.TestCase):
    def test_vm_exception_attributed_to_first_frame_of_ours(self):
        log = ('Log x started at 2026-11-03 19:14:37 (UTC)\n'
               '19:14:37.665 ENGINE       : Initializing engine, version 192142\n'
               '20:10:57.005   SCRIPT    (E): Virtual Machine Exception\n\nReason: NULL pointer to instance\n\n'
               "Class:      'SCR_AICombatComponent'\nFunction: 'GetCombatMode'\nStack trace:\n"
               'Scripts/Game/AI/Components/SCR_AICombatComponent.c:855 Function GetCombatMode\n'
               'Scripts/Game/AIO/AIO_Maneuvers.c:29 Function GetCombatMode\n'
               'Scripts/Game/BBB/BBB_Engagement.c:51 Function Ready\n'
               '20:10:57.498 SCRIPT       : [ACR] next line\n'
               '20:11:00.000 SCRIPT    (E): Virtual Machine Exception\n\nReason: NULL pointer to instance\n\n'
               "Class:      'SCR_AICombatComponent'\nFunction: 'GetCombatMode'\nStack trace:\n"
               'Scripts/Game/AI/Components/SCR_AICombatComponent.c:855 Function GetCombatMode\n'
               'Scripts/Game/AIO/AIO_Maneuvers.c:29 Function GetCombatMode\n'
               'Scripts/Game/BBB/BBB_Engagement.c:51 Function Ready\n'
               '20:11:01.000 SCRIPT       : end\n')
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'console.log'
            p.write_text(log, encoding='utf-8')
            s = playlog.parse_session('client', p)
        self.assertEqual(len(s['errors']), 1)
        e = next(iter(s['errors'].values()))
        self.assertEqual((e['mod'], e['mods'], e['count']), ('AiO', ['AiO', 'BBB'], 2))
        self.assertEqual(s['engine'], '192142')


if __name__ == '__main__':
    unittest.main()
