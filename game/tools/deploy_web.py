"""Stage the new portrait package and dual launcher; preserve the legacy landscape bytes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def hashes(directory):
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-root', type=Path, default=Path(r'G:\Coding\Projects\Akil0814.github.io'))
    args = parser.parse_args()
    site = args.site_root.resolve(strict=True)
    target = site / 'projects/FlyMe2TheMoon/web'
    landscape = target / 'runtime'
    if not (landscape / 'manifest.json').is_file():
        raise RuntimeError('The existing landscape runtime must already be present')
    original = hashes(landscape)
    runtime = ROOT / 'out/web-portrait/runtime'
    manifest = json.loads((runtime / 'manifest.json').read_text())
    if (manifest.get('displayMode'), manifest.get('logicalWidth'), manifest.get('logicalHeight')) != ('portrait', 640, 960):
        raise RuntimeError('Expected the current 640x960 portrait package')
    for name, entry in manifest['files'].items():
        path = runtime / name
        if not path.resolve().is_relative_to(runtime.resolve()):
            raise RuntimeError('Runtime path leaves package directory')
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256'] or path.stat().st_size != entry['bytes']:
            raise RuntimeError('Package integrity check failed: ' + name)
    page = ROOT / 'game/platform/web'
    mappings = [(page / name, target / name) for name in
                ['index.html', 'flyme2themoon_web.css', 'flyme2themoon_web.js', 'platform.js', '.gitattributes']]
    mappings += [(page / f'i18n/{lang}/flyme2themoon_web.json', site / f'i18n/{lang}/flyme2themoon_web.json') for lang in ['en', 'zh', 'ja']]
    mappings += [(ROOT / 'docs/development/web-build.md', target / 'BUILD.md')]
    backup = ROOT / 'out/web-portrait/backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    for name in ['runtime', 'runtime-portrait']:
        if (target / name).is_dir():
            shutil.copytree(target / name, backup / 'projects/FlyMe2TheMoon/web' / name)
    for _, destination in mappings:
        if destination.is_file():
            previous = backup / destination.relative_to(site)
            previous.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(destination, previous)
    (backup / 'landscape-hashes.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf8')
    for source, destination in mappings:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    shutil.copytree(runtime, target / 'runtime-portrait', dirs_exist_ok=True)
    if hashes(landscape) != original:
        raise RuntimeError('Existing landscape files changed unexpectedly')
    print('Staged dual-version page at', target)
    print('Landscape unchanged:', len(original), 'files')
    print('Backup:', backup)


if __name__ == '__main__':
    main()
