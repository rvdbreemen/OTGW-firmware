#!/usr/bin/env python3
"""Verify native asset loading without accepting JavaScript recovery as success.

Run with an explicit device URL (or a transparent SSH tunnel):
  uv run --with playwright python scripts/tests/asset_loading.py --base-url http://127.0.0.1:18765 --browser all
Uses installed Chrome when available; Firefox/WebKit need Playwright browsers.
Outputs JSON; exit 0 = clean native cold/warm loads, 1 = acceptance failure,
2 = probe infrastructure failure. Does not alter device configuration.
"""
import argparse
import asyncio
import json
from pathlib import Path
import shutil
import sys
from urllib.parse import urlsplit

ASSETS = ('v2-bundle.css', 'v2.js', 'fonts/inter-400.woff2',
          'fonts/inter-700.woff2', 'fonts/jetbrains-mono-400.woff2')
EVENTS = {'asset_retry', 'asset_recovered', 'asset_failed', 'asset_load_summary'}
GOAL = 'Normal native asset loading succeeds with zero JavaScript retries.'


def assess_load(record, network):
    """Judge observed behavior independently of the page's success flag."""
    problems = []
    def reject(kind, asset=None, **details):
        problems.append({'kind': kind, **({'asset': asset} if asset else {}), **details})
    if not isinstance(record, dict):
        return {'passed': False, 'problems': [{'kind': 'missing_diagnostics'}]}
    if record.get('mode') != 'native':
        reject('not_native')
    if record.get('complete') is not True:
        reject('incomplete')
    if record.get('cleanFirstAttempt') is not True:
        reject('not_clean')
    for field, kind in (('retryCount', 'retry_used'), ('failureCount', 'failed_attempt')):
        count = record.get(field)
        if type(count) is not int or count < 0:
            reject('invalid_counter', field=field)
        elif count:
            reject(kind, count=count)
    entries = record.get('assets')
    if not isinstance(entries, dict) or set(entries) != set(ASSETS):
        reject('asset_inventory')
        entries = entries if isinstance(entries, dict) else {}
    for asset in ASSETS:
        entry = entries.get(asset, {})
        if (not isinstance(entry, dict) or entry.get('state') != 'loaded'
                or type(entry.get('attempts')) is not int or entry.get('attempts') != 1
                or type(entry.get('retries')) is not int or entry.get('retries') != 0):
            reject('not_first_attempt', asset)
        observed = network.get(asset, {})
        if observed.get('requests', 0) == 0:
            reject('asset_not_observed', asset)
        elif observed['requests'] != 1:
            reject('duplicate_asset_request', asset, count=observed['requests'])
        if not observed.get('statuses'):
            reject('asset_response_missing', asset)
    for asset, observed in network.items():
        for status in observed.get('statuses', []):
            if not (200 <= status < 300 or status == 304):
                reject('asset_http_failure', asset, status=status)
        if observed.get('errors'):
            reject('asset_network_failure', asset, errors=observed['errors'])
    return {'passed': not problems, 'problems': problems}


async def load_once(context, base, browser_name, mode):
    page = await context.new_page()
    network = {asset: {'requests': 0, 'statuses': [], 'errors': []} for asset in ASSETS}
    events = []
    console_tasks = set()
    origin = urlsplit(base)
    def asset_name(url):
        parsed = urlsplit(url)
        name = parsed.path.lstrip('/')
        return name if parsed.netloc == origin.netloc and name in (*ASSETS, 'favicon.ico') else None
    def observation(url):
        name = asset_name(url)
        if name is None:
            return None
        return network.setdefault(name, {'requests': 0, 'statuses': [], 'errors': []})
    def requested(request):
        entry = observation(request.url)
        if entry is not None:
            entry['requests'] += 1
    def responded(response):
        entry = observation(response.url)
        if entry is not None and len(entry['statuses']) < 16:
            entry['statuses'].append(response.status)
    def failed(request):
        entry = observation(request.url)
        if entry is not None and len(entry['errors']) < 16:
            entry['errors'].append(request.failure or 'network failure')
    async def capture_console(message):
        try:
            name, detail = await asyncio.gather(*(arg.json_value() for arg in message.args[:2]))
        except Exception:
            return  # The document may have closed after the console event.
        if name in EVENTS and isinstance(detail, dict):
            events.append({'event': name, **detail})
    def logged(message):
        if len(events) + len(console_tasks) >= 64 or message.text.split(' ', 1)[0] not in EVENTS:
            return
        task = asyncio.create_task(capture_console(message))
        console_tasks.add(task)
        task.add_done_callback(console_tasks.discard)
    page.on('request', requested)
    page.on('response', responded)
    page.on('requestfailed', failed)
    page.on('console', logged)
    problem = None
    try:
        try:
            await page.goto(base, wait_until='domcontentloaded', timeout=25000)
            await page.wait_for_function('window.otgwAssetLoad && (window.otgwAssetLoad.complete || window.otgwAssetLoad.retryCount > 0 || window.otgwAssetLoad.failureCount > 0)', timeout=20000)
        except Exception as error:
            problem = type(error).__name__
        record = await page.evaluate('window.otgwAssetLoad ? JSON.parse(JSON.stringify(window.otgwAssetLoad)) : null')
        timing = await page.evaluate("""(() => {
          const css = performance.getEntriesByType('resource').find(r => new URL(r.name).pathname === '/v2-bundle.css');
          const paint = performance.getEntriesByName('first-contentful-paint')[0];
          return {cssEndMs: css ? Math.round(css.responseEnd) : null,
            firstPaintMs: paint ? Math.round(paint.startTime) : null,
            paintAfterCss: css && paint ? paint.startTime >= css.responseEnd : null};
        })()""")
        # Some headless engines do not fetch browser-chrome favicons. Check that
        # endpoint once after the native critical assets, without JavaScript retry.
        if record and record.get('complete') and record.get('retryCount') == 0 and 'favicon.ico' not in network:
            response = await context.request.get(base + 'favicon.ico', timeout=10000)
            network['favicon.ico'] = {'requests': 1, 'statuses': [response.status], 'errors': []}
        verdict = assess_load(record, network)
        if problem:
            verdict['problems'].append({'kind': 'probe_timeout_or_navigation_failure', 'error': problem})
            verdict['passed'] = False
        if timing['paintAfterCss'] is False:
            verdict['problems'].append({'kind': 'paint_before_css'})
            verdict['passed'] = False
        if console_tasks:
            await asyncio.gather(*list(console_tasks), return_exceptions=True)
        if record and record.get('complete'):
            if sum(event['event'] == 'asset_load_summary' for event in events) != 1:
                verdict['problems'].append({'kind': 'missing_or_duplicate_summary'})
                verdict['passed'] = False
        if any(event['event'] in ('asset_retry', 'asset_recovered', 'asset_failed') for event in events):
            verdict['problems'].append({'kind': 'recovery_event_observed'})
            verdict['passed'] = False
        return {'browser': browser_name, 'mode': mode, **verdict, 'diagnostics': record,
                'network': network, 'events': events, 'timing': timing}
    finally:
        await page.close()


async def probe(args):
    from playwright.async_api import async_playwright
    runs = []
    async with async_playwright() as playwright:
        names = ('chromium', 'firefox', 'webkit') if args.browser == 'all' else (args.browser,)
        for name in names:
            options = {}
            if name == 'chromium' and (Path('/Applications/Google Chrome.app').exists() or shutil.which('google-chrome')):
                options['channel'] = 'chrome'
            browser = await getattr(playwright, name).launch(**options)
            try:
                for number in range(args.rounds):
                    context = await browser.new_context()
                    try:
                        for mode in ('cold', 'warm'):
                            run = await load_once(context, args.base_url, name, mode)
                            run['round'] = number + 1
                            runs.append(run)
                            if not run['passed']:
                                return {'goal': GOAL, 'passed': False, 'runs': runs}
                    finally:
                        await context.close()
            finally:
                await browser.close()
    return {'goal': GOAL, 'passed': True, 'runs': runs}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--browser', choices=('chromium', 'firefox', 'webkit', 'all'), default='chromium')
    parser.add_argument('--rounds', type=int, default=3)
    args = parser.parse_args(argv)
    parsed = urlsplit(args.base_url)
    if parsed.scheme != 'http' or not parsed.hostname or parsed.username or parsed.password or parsed.query or parsed.fragment or parsed.path not in ('', '/'):
        parser.error('--base-url must be a plain HTTP origin without credentials, path, query or fragment')
    if not 1 <= args.rounds <= 20:
        parser.error('--rounds must be between 1 and 20')
    args.base_url = args.base_url.rstrip('/') + '/'
    try:
        result = asyncio.run(probe(args))
    except Exception as error:
        print(json.dumps({'goal': GOAL, 'passed': False, 'infrastructure_error': type(error).__name__, 'detail': str(error)}))
        return 2
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
