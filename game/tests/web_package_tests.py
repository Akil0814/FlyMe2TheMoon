"""Verify actual packaged bytes, virtual paths, hashes, and localized web copy."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
runtime = ROOT / 'out/web-portrait/runtime'
manifest = json.loads((runtime / 'manifest.json').read_text())
assets = json.loads((runtime / 'asset-manifest.json').read_text())
expected_version = re.search(r'project\(FlyMe2TheMoon\s+VERSION\s+(\d+\.\d+\.\d+)',
                             (ROOT / 'CMakeLists.txt').read_text(encoding='utf8'))[1]
assert manifest['version'] == 1 and manifest['gameVersion'] == expected_version
assert (manifest['displayMode'],manifest['logicalWidth'],manifest['logicalHeight']) == ('portrait',640,960)
seen = set()
for package in manifest['packages']:
    data = (runtime / package['data']).read_bytes()
    assert len(data) == package['bytes'] <= 32 * 1024 * 1024
    script = (runtime / package['script']).read_text()
    metadata = json.JSONDecoder().raw_decode(script[script.rindex('loadPackage(') + len('loadPackage('):])[0]
    for entry in metadata['files']:
        virtual = entry['filename'].lstrip('/')
        assert virtual in assets and virtual not in seen, virtual
        seen.add(virtual)
        content = data[entry['start']:entry['end']]
        assert len(content) == assets[virtual]['bytes']
        assert hashlib.sha256(content).hexdigest() == assets[virtual]['sha256']
        assert content == (ROOT / virtual).read_bytes(), virtual
assert seen == set(assets)
assert 'assets/.elysia_root' in seen
assert len(json.loads((ROOT / 'assets/configs/catalog.json').read_text())['dependencies']) == 60
for name, entry in manifest['files'].items():
    content = (runtime / name).read_bytes()
    assert len(content) == entry['bytes']
    assert hashlib.sha256(content).hexdigest() == entry['sha256']
locales = []
for lang in ['en', 'zh', 'ja']:
    page = json.loads((ROOT / f'game/platform/web/i18n/{lang}/flyme2themoon_web.json').read_text(encoding='utf8'))['flyme2themoon_web']
    keys = set(page['runtime']) | {'actions.' + key for key in page['actions']}
    locales.append(keys)
assert locales[0] == locales[1] == locales[2]
print(f'PASS: {len(seen)} assets match source bytes, all 60 levels included, {len(manifest["packages"])} packages <=32 MiB, runtime hashes and locale keys valid')
