"""The native-loading acceptance tool must reject recovery disguised as success."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ('v2-bundle.css', 'v2.js', 'fonts/inter-400.woff2',
          'fonts/inter-700.woff2', 'fonts/jetbrains-mono-400.woff2')


def clean_record():
    return {'mode': 'native', 'complete': True, 'cleanFirstAttempt': True,
            'retryCount': 0, 'failureCount': 0,
            'assets': {name: {'attempts': 1, 'retries': 0, 'state': 'loaded'} for name in ASSETS}}


def network():
    return {name: {'requests': 1, 'statuses': [200], 'errors': []} for name in ASSETS}


class AssetAcceptanceTests(unittest.TestCase):
    def verdict(self, record=None, requests=None):
        path = ROOT / 'scripts/tests/asset_loading.py'
        spec = importlib.util.spec_from_file_location('asset_loading_acceptance', path)
        module = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(module)
        except FileNotFoundError:
            self.fail('The native-loading acceptance evaluator is not implemented')
        return module.assess_load(clean_record() if record is None else record,
                                  network() if requests is None else requests)

    def kinds(self, verdict):
        return {problem['kind'] for problem in verdict['problems']}

    def test_native_first_attempt_success_is_accepted(self):
        result = self.verdict()
        self.assertTrue(result['passed'])
        self.assertEqual(result['problems'], [])

    def test_recovered_load_is_not_clean_even_if_page_claims_success(self):
        record = clean_record()
        record['retryCount'] = 1
        record['failureCount'] = 1
        record['assets']['v2.js'].update(attempts=2, retries=1)
        result = self.verdict(record)
        self.assertFalse(result['passed'])
        self.assertIn('retry_used', self.kinds(result))
        self.assertIn('failed_attempt', self.kinds(result))
        self.assertIn('not_first_attempt', self.kinds(result))

    def test_incomplete_and_managed_modes_are_rejected(self):
        for field, value, kind in [('complete', False, 'incomplete'),
                                    ('mode', 'managed', 'not_native'),
                                    ('cleanFirstAttempt', False, 'not_clean')]:
            with self.subTest(field=field):
                record = clean_record()
                record[field] = value
                result = self.verdict(record)
                self.assertFalse(result['passed'])
                self.assertIn(kind, self.kinds(result))

    def test_asset_refusal_is_rejected_even_if_diagnostics_claim_clean(self):
        requests = network()
        requests['v2-bundle.css']['statuses'] = [503]
        result = self.verdict(requests=requests)
        self.assertFalse(result['passed'])
        self.assertIn('asset_http_failure', self.kinds(result))

    def test_request_without_a_response_cannot_be_reported_clean(self):
        requests = network()
        requests['v2.js']['statuses'] = []
        result = self.verdict(requests=requests)
        self.assertFalse(result['passed'])
        self.assertIn('asset_response_missing', self.kinds(result))

    def test_network_error_and_duplicate_fetch_are_rejected(self):
        requests = network()
        requests['v2.js']['errors'] = ['connection reset']
        requests['fonts/inter-400.woff2']['requests'] = 2
        requests['fonts/inter-400.woff2']['statuses'] = [200, 200]
        result = self.verdict(requests=requests)
        self.assertFalse(result['passed'])
        self.assertIn('asset_network_failure', self.kinds(result))
        self.assertIn('duplicate_asset_request', self.kinds(result))

    def test_cached_revalidation_is_not_mistaken_for_failure(self):
        requests = network()
        for entry in requests.values():
            entry['statuses'] = [304]
        self.assertTrue(self.verdict(requests=requests)['passed'])

    def test_missing_assets_and_invalid_counters_cannot_pass(self):
        record = clean_record()
        del record['assets']['v2.js']
        record['retryCount'] = None
        result = self.verdict(record)
        self.assertFalse(result['passed'])
        self.assertIn('asset_inventory', self.kinds(result))
        self.assertIn('invalid_counter', self.kinds(result))

    def test_failed_favicon_is_also_rejected(self):
        requests = network()
        requests['favicon.ico'] = {'requests': 1, 'statuses': [503], 'errors': []}
        result = self.verdict(requests=requests)
        self.assertFalse(result['passed'])
        self.assertIn('asset_http_failure', self.kinds(result))


if __name__ == '__main__':
    unittest.main()
