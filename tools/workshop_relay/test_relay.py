import base64
import copy
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch
import urllib.error

import relay as r

ID = 'AC230926A0010001'
IDS = [ID, 'BBB0261015000001', '629405AEFC950C12']
FIXTURES = Path(__file__).parent / 'fixtures'


def fixture(mod_id, suffix):
    return json.loads((FIXTURES / f'{mod_id}-{suffix}.json').read_text(encoding='utf-8'))


class FakeHTTP:
    def __init__(self):
        self.responses = {}
        self.posts = []
        self.post_error = None
        for mod_id in IDS:
            base = f'{r.API}/mods/{mod_id}'
            self.responses[base] = fixture(mod_id, 'mod')
            self.responses[base + '/versions'] = fixture(mod_id, 'versions')
            detail = fixture(mod_id, 'detail')
            self.responses[base + '/versions/' + detail['data']['version']] = detail

    def request(self, method, url, headers=None, payload=None):
        if method == 'POST':
            self.posts.append(copy.deepcopy(payload))
            if self.post_error:
                raise self.post_error
            return {'id': str(len(self.posts))}, {}
        value = self.responses[url]
        if isinstance(value, Exception):
            raise value
        return copy.deepcopy(value), {}

    def add_version(self, mod_id=ID, version='0.5.7', notes='Exact new notes\n'):
        base = f'{r.API}/mods/{mod_id}'
        row = {'version': version, 'published': True, 'approved': True, 'createdAt': '2026-10-24T01:00:00Z'}
        self.responses[base + '/versions']['data']['versions'].insert(0, row)
        self.responses[base + '/versions']['data']['count'] += 1
        self.responses[base]['mod']['versionCount'] += 1
        self.responses[base]['mod']['version'] = version
        self.responses[base + '/versions/' + version] = {'status': 'success', 'data': {'modId': mod_id, **row, 'changelog': notes}}


class Store:
    def __init__(self, fail_at=None):
        self.durable = {'schema': 1, 'mods': {}}
        self.saves = 0
        self.fail_at = fail_at

    def save(self, state):
        self.saves += 1
        if self.saves == self.fail_at:
            raise r.StateError('simulated durable write failure')
        self.durable = copy.deepcopy(state)


class RelayTests(unittest.TestCase):
    def setUp(self):
        self.sleep = patch('relay.time.sleep').start()
        self.addCleanup(patch.stopall)
        self.http, self.store = FakeHTTP(), Store()
        self.workshop = r.Workshop(self.http)

    def run_poll(self, ids=None, initialize=False):
        return r.poll(ids or [ID], copy.deepcopy(self.store.durable), self.store,
                      self.workshop, self.http, 'not-a-real-webhook', initialize)

    def baseline(self, ids=None):
        self.assertEqual(self.run_poll(ids, True), 0)
        self.assertEqual(self.http.posts, [])

    def test_live_fixtures_parse_all_mods_and_exact_notes(self):
        for mod_id in IDS:
            mod, rows = self.workshop.snapshot(mod_id)
            notes = self.workshop.detail(mod_id, mod['version'])
            self.assertEqual(notes, fixture(mod_id, 'detail')['data']['changelog'])
            self.assertEqual(len(rows), mod['versionCount'])

    def test_baseline_and_repeated_poll_are_silent(self):
        self.baseline(IDS)
        self.assertEqual(self.run_poll(IDS), 0)
        self.assertEqual(self.http.posts, [])
        self.assertEqual(self.store.saves, 3)

    def test_exact_new_release_and_restart_dedup(self):
        self.baseline()
        notes = '  @everyone\n```text\n' + ('hello 😀\r\n' * 700) + '```\n '
        self.http.add_version(notes=notes)
        self.assertEqual(self.run_poll(), 0)
        bodies = [p['content'].split('\n\n', 1)[1] for p in self.http.posts]
        self.assertEqual(''.join(bodies), notes)
        self.assertTrue(all(r.units(p['content']) <= 2000 for p in self.http.posts))
        self.assertTrue(all(p['allowed_mentions']['parse'] == [] for p in self.http.posts))
        count = len(self.http.posts)
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), count)

    def test_metadata_edit_is_not_release(self):
        self.baseline()
        self.http.responses[f'{r.API}/mods/{ID}']['mod']['updatedAt'] = '2099-01-01T00:00:00Z'
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(self.http.posts, [])

    def test_multiple_new_versions_including_lower_label(self):
        self.baseline()
        self.http.add_version(version='0.5.7')
        self.http.add_version(version='0.0.1')
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), 2)

    def test_unpublished_becomes_published(self):
        self.http.add_version()
        base = f'{r.API}/mods/{ID}'
        self.http.responses[base + '/versions']['data']['versions'][0]['published'] = False
        self.http.responses[base]['mod']['version'] = '0.5.6'
        self.baseline()
        self.http.responses[base + '/versions']['data']['versions'][0]['published'] = True
        self.http.responses[base]['mod']['version'] = '0.5.7'
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), 1)

    def test_missing_notes_wait_then_deliver(self):
        self.baseline()
        self.http.add_version(notes='')
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(self.http.posts, [])
        self.http.responses[f'{r.API}/mods/{ID}/versions/0.5.7']['data']['changelog'] = 'Now available'
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), 1)

    def test_detail_identity_and_publication_checks(self):
        self.baseline()
        self.http.add_version()
        detail = self.http.responses[f'{r.API}/mods/{ID}/versions/0.5.7']['data']
        for field, value in [('modId', IDS[1]), ('version', 'wrong'), ('published', False), ('approved', False)]:
            old = detail[field]
            detail[field] = value
            self.assertEqual(self.run_poll(), 1)
            self.assertEqual(self.http.posts, [])
            detail[field] = old

    def test_api_outage_leaves_ledger_and_other_mod_works(self):
        self.baseline(IDS)
        before = copy.deepcopy(self.store.durable['mods'][ID])
        self.http.responses[f'{r.API}/mods/{ID}'] = r.HttpError(503)
        self.http.add_version(IDS[1], '0.1.6')
        self.assertEqual(self.run_poll(IDS), 1)
        self.assertEqual(before, self.store.durable['mods'][ID])
        self.assertEqual(len(self.http.posts), 1)

    def test_incomplete_history_does_not_initialize(self):
        self.http.responses[f'{r.API}/mods/{ID}/versions']['data']['count'] += 1
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(self.store.durable['mods'], {})

    def test_duplicate_history_is_rejected(self):
        data = self.http.responses[f'{r.API}/mods/{ID}/versions']['data']
        data['versions'][-1] = data['versions'][0]
        with self.assertRaises(r.RelayError):
            self.workshop.snapshot(ID)

    def test_pagination_complete(self):
        url = f'{r.API}/mods/{ID}/versions'
        original = self.http.responses[url]
        page2 = copy.deepcopy(original)
        page2['data']['versions'] = page2['data']['versions'][3:]
        original['data']['versions'] = original['data']['versions'][:3]
        original['links'] = {'next': '?page=2'}
        self.http.responses[url + '?page=2'] = page2
        self.assertEqual(len(self.workshop.snapshot(ID)[1]), 7)

    def test_bad_pagination_does_not_send_credentials(self):
        url = f'{r.API}/mods/{ID}/versions'
        for next_url in ['https://evil.example/path', url]:
            self.http.responses[url]['links'] = {'next': next_url}
            with self.assertRaises(r.RelayError):
                self.workshop.snapshot(ID)

    def test_stale_cache_is_rejected(self):
        with patch.object(self.http, 'request', return_value=({'status': 'success'}, {'X-Cache': 'STALE'})):
            with self.assertRaises(r.RelayError):
                self.workshop.get(r.API)

    def test_cache_mismatch_leaves_baseline_unchanged(self):
        self.http.responses[f'{r.API}/mods/{ID}']['mod']['versionCount'] += 1
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(self.store.saves, 0)

    def test_failure_at_each_checkpoint_never_automatically_duplicates(self):
        # Saves: baseline, plan, intent, acknowledgement, completion.
        for fail_at in range(2, 6):
            with self.subTest(fail_at=fail_at):
                self.http, self.store = FakeHTTP(), Store(fail_at)
                self.workshop = r.Workshop(self.http)
                self.baseline()
                self.http.add_version()
                with self.assertRaises(r.StateError):
                    self.run_poll()
                self.store.fail_at = None
                self.run_poll()
                self.assertLessEqual(len(self.http.posts), 1)

    def test_uncertain_send_blocks_restart_until_recovery(self):
        self.baseline()
        self.http.add_version(notes='x' * 3500)
        self.http.post_error = r.RelayError('simulated lost acknowledgement')
        self.assertEqual(self.run_poll(), 1)
        self.http.post_error = None
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(len(self.http.posts), 1)
        state = copy.deepcopy(self.store.durable)
        r.recover(state, self.store, ID, '0.5.7', 1, 'delivered')
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), 3)
        self.assertEqual(self.run_poll(), 0)
        self.assertEqual(len(self.http.posts), 3)

    def test_wrong_recovery_and_retry_recovery(self):
        self.baseline()
        self.http.add_version()
        self.http.post_error = r.HttpError(500)
        self.run_poll()
        for version, part in [('0.5.6', 1), ('0.5.7', 2)]:
            with self.assertRaises(r.RelayError):
                r.recover(copy.deepcopy(self.store.durable), self.store, ID, version, part, 'retry')
        r.recover(copy.deepcopy(self.store.durable), self.store, ID, '0.5.7', 1, 'retry')
        self.http.post_error = None
        self.assertEqual(self.run_poll(), 0)

    def test_applied_but_unacknowledged_state_writes_are_safe(self):
        for fail_at in range(2, 6):
            with self.subTest(fail_at=fail_at):
                self.http, self.store = FakeHTTP(), Store()
                self.workshop = r.Workshop(self.http)
                self.baseline()
                self.http.add_version()
                real_save = self.store.save
                def uncertain_save(state):
                    real_save(state)
                    if self.store.saves == fail_at:
                        raise r.StateError('write applied but acknowledgement lost')
                with patch.object(self.store, 'save', side_effect=uncertain_save):
                    with self.assertRaises(r.StateError):
                        self.run_poll()
                self.run_poll()
                self.assertLessEqual(len(self.http.posts), 1)

    def test_invalid_discord_acknowledgement_holds_intent(self):
        self.baseline()
        self.http.add_version()
        original = self.http.request
        def invalid_ack(method, url, headers=None, payload=None):
            if method == 'POST':
                self.http.posts.append(payload)
                return {'not_a_message': True}, {}
            return original(method, url, headers, payload)
        with patch.object(self.http, 'request', side_effect=invalid_ack):
            self.assertEqual(self.run_poll(), 1)
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(len(self.http.posts), 1)

    def test_definite_rejection_is_retryable(self):
        self.baseline()
        self.http.add_version()
        self.http.post_error = r.HttpError(429)
        self.assertEqual(self.run_poll(), 1)
        self.assertIsNone(self.store.durable['mods'][ID]['pending']['inflight'])
        self.http.post_error = None
        self.assertEqual(self.run_poll(), 0)

    def test_http_408_is_held_as_uncertain(self):
        self.baseline()
        self.http.add_version()
        self.http.post_error = r.HttpError(408)
        self.assertEqual(self.run_poll(), 1)
        self.http.post_error = None
        self.assertEqual(self.run_poll(), 1)
        self.assertEqual(len(self.http.posts), 1)

    def test_long_unicode_header_still_fits_discord(self):
        payloads = r.messages(ID, '😀' * 100, '😀' * 100, '😀' * 10000)
        self.assertTrue(all(r.units(p['content']) <= 2000 for p in payloads))

    def test_initialize_existing_ledger_never_consumes_new_release(self):
        self.baseline()
        self.http.add_version()
        self.assertEqual(self.run_poll(initialize=True), 0)
        self.assertNotIn('0.5.7', self.store.durable['mods'][ID]['seen'])
        self.assertEqual(self.run_poll(), 0)

    def test_new_mod_initializes_without_historical_spam(self):
        self.baseline()
        self.assertEqual(self.run_poll(IDS), 0)
        self.assertEqual(self.http.posts, [])

    def test_unicode_and_unbroken_long_line_lossless(self):
        for text in ['😀' * 10000, 'a' * 10000, '\r\n' * 5000, '汉字e\u0301' * 3000]:
            chunks = r.split_text(text)
            self.assertEqual(''.join(chunks), text)
            self.assertTrue(all(r.units(c) <= 1600 for c in chunks))

    def test_webhook_validation_and_wait(self):
        url = r.webhook_url('https://discord.com/api/webhooks/123/test-token?thread_id=456&wait=false')
        self.assertIn('wait=true', url)
        self.assertIn('thread_id=456', url)
        for value in ['', 'http://discord.com/api/webhooks/123/token', 'https://evil.example/api/webhooks/123/token',
                      'https://discord.com@evil.example/api/webhooks/123/token']:
            with self.assertRaises(r.RelayError):
                r.webhook_url(value)

    def test_webhook_preflight_verifies_identity_without_post_or_token_logging(self):
        url = 'https://discord.com/api/webhooks/123/test-token?wait=true&thread_id=456'
        with patch.object(self.http, 'request', return_value=({'id': '123', 'type': 1, 'token': 'must-not-log'}, {})) as request:
            with patch('sys.stdout', new_callable=io.StringIO) as output:
                r.verify_webhook(self.http, url)
            request.assert_called_once_with('GET', 'https://discord.com/api/webhooks/123/test-token')
            self.assertNotIn('must-not-log', output.getvalue())
        for result in [{'id': 'wrong', 'type': 1}, {'id': '123', 'type': 2}]:
            with patch.object(self.http, 'request', return_value=(result, {})):
                with self.assertRaises(r.RelayError):
                    r.verify_webhook(self.http, url)

    def test_corrupt_entry_does_not_rebaseline(self):
        for entry in [{}, {'seen': 'wrong', 'pending': None}, {'seen': [], 'pending': {}}]:
            with self.assertRaises(r.StateError):
                r.validate_state({'schema': 1, 'mods': {ID: entry}})


class Response:
    status = 200
    headers = {}
    def __init__(self, body=b'{"id":"123"}'):
        self.body = body
    def __enter__(self):
        return self
    def __exit__(self, *args):
        pass
    def read(self):
        return self.body


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.http = r.HTTP()
        self.sleep = patch('relay.time.sleep').start()
        self.addCleanup(patch.stopall)

    def http_error(self, code, headers=None, body=b'{}'):
        return urllib.error.HTTPError('https://example.invalid/private-token', code, 'error', headers or {}, io.BytesIO(body))

    def test_discord_429_retries_same_payload_with_retry_after(self):
        with patch.object(self.http.opener, 'open', side_effect=[self.http_error(429, {'Retry-After': '3'}), Response()]) as send:
            result, _ = self.http.request('POST', 'https://example.invalid', payload={'content': 'same'})
            self.assertEqual(result['id'], '123')
            self.assertEqual(send.call_args_list[0].args[0].data, send.call_args_list[1].args[0].data)
            self.sleep.assert_called_once_with(3)

    def test_discord_500_is_not_retried(self):
        with patch.object(self.http.opener, 'open', side_effect=self.http_error(500)) as send:
            with self.assertRaises(r.HttpError):
                self.http.request('POST', 'https://example.invalid', payload={})
            self.assertEqual(send.call_count, 1)

    def test_read_outages_and_202_retry(self):
        for error in [self.http_error(503), self.http_error(202), urllib.error.URLError('secret')]:
            with patch.object(self.http.opener, 'open', side_effect=[error, Response()]) as send:
                self.http.request('GET', 'https://example.invalid')
                self.assertEqual(send.call_count, 2)

    def test_network_error_redacts_token_and_is_not_retried_on_post(self):
        with patch.object(self.http.opener, 'open', side_effect=urllib.error.URLError('private-token')) as send:
            with self.assertRaises(r.RelayError) as result:
                self.http.request('POST', 'https://example.invalid/private-token', payload={})
            self.assertNotIn('private-token', str(result.exception))
            self.assertEqual(send.call_count, 1)

    def test_large_retry_delay_does_not_hammer_service(self):
        with patch.object(self.http.opener, 'open', side_effect=self.http_error(429, {'Retry-After': '86400'})) as send:
            with self.assertRaises(r.RelayError):
                self.http.request('GET', 'https://example.invalid')
            self.assertEqual(send.call_count, 1)
            self.sleep.assert_not_called()

    def test_retry_after_date_and_discord_json_seconds(self):
        self.assertEqual(r.delay({}, b'{"retry_after":1.25}', 0), 1.25)
        date = (r.dt.datetime.now(r.dt.timezone.utc) + r.dt.timedelta(seconds=15)).strftime('%a, %d %b %Y %H:%M:%S GMT')
        self.assertTrue(13 <= r.delay({'Retry-After': date}, b'{}', 0) <= 15)

    def test_state_cas_sha_and_ambiguous_write_no_retry(self):
        store = r.GitHubState(self.http, 'owner/repo', 'not-a-real-token')
        store.sha = 'old-blob'
        with patch.object(self.http.opener, 'open', return_value=Response(b'{"content":{"sha":"new-blob"}}')) as send:
            store.save({'schema': 1, 'mods': {}})
            payload = json.loads(send.call_args.args[0].data)
            self.assertEqual(payload['sha'], 'old-blob')
            self.assertEqual(store.sha, 'new-blob')
            self.assertEqual(json.loads(base64.b64decode(payload['content'])), {'schema': 1, 'mods': {}})
        with patch.object(self.http.opener, 'open', side_effect=self.http_error(409)) as send:
            with self.assertRaises(r.StateError):
                store.save({})
            self.assertEqual(send.call_count, 1)

    def test_state_missing_without_initialize_refuses_reset(self):
        store = r.GitHubState(self.http, 'owner/repo', 'not-a-real-token')
        with patch.object(self.http.opener, 'open', side_effect=self.http_error(404)) as send:
            with self.assertRaises(r.StateError):
                store.load()
            self.assertEqual(send.call_count, 1)

    def test_state_bootstrap_and_load_real_envelope(self):
        state = {'schema': 1, 'mods': {ID: {'seen': ['0.5.6'], 'pending': None}}}
        store = r.GitHubState(self.http, 'owner/repo', 'not-a-real-token')
        envelope = json.dumps({'sha': 'stored-blob', 'content': base64.b64encode(json.dumps(state).encode()).decode()}).encode()
        with patch.object(self.http.opener, 'open', side_effect=[Response(), Response(envelope)]):
            self.assertEqual(store.load(), state)
            self.assertEqual(store.sha, 'stored-blob')
        with patch.dict('os.environ', {'GITHUB_SHA': 'base-commit'}):
            with patch.object(self.http.opener, 'open', side_effect=[self.http_error(404), Response(), self.http_error(404)]) as send:
                self.assertEqual(store.load(initialize=True), {'schema': 1, 'mods': {}})
                request = send.call_args_list[1].args[0]
                self.assertEqual(json.loads(request.data), {'ref': 'refs/heads/' + r.BRANCH, 'sha': 'base-commit'})

    def test_oversized_ledger_rejected_before_network_write(self):
        store = r.GitHubState(self.http, 'owner/repo', 'not-a-real-token')
        with patch.object(self.http.opener, 'open') as send:
            with self.assertRaises(r.StateError):
                store.save({'oversized': 'x' * 900001})
            send.assert_not_called()


if __name__ == '__main__':
    unittest.main()
