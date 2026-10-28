"""Copy a validated Web package into the existing portfolio checkout. No Git changes."""
import argparse
import json
from pathlib import Path
import shutil
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-root', type=Path, default=Path(r'G:\Coding\Projects\Akil0814.github.io'))
    args = parser.parse_args()
    site = args.site_root.resolve(strict=True)
    if not (site / 'projects/FlyMe2TheMoon/FlyMe2TheMoon.html').is_file():
        raise RuntimeError('Expected existing FlyMe2TheMoon project page')
    runtime = ROOT / 'out/web/runtime'
    manifest = json.loads((runtime / 'manifest.json').read_text())
    import hashlib
    for name, entry in manifest['files'].items():
        path = runtime / name
        with path.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if actual != entry['sha256'] or path.stat().st_size != entry['bytes']:
            raise RuntimeError('Package integrity check failed: ' + name)
    page = ROOT / 'game/platform/web'
    target = site / 'projects/FlyMe2TheMoon/web'
    mappings = [(page / name, target / name) for name in ['index.html', 'flyme2themoon_web.css', 'flyme2themoon_web.js', '.gitattributes']]
    mappings += [(page / f'i18n/{lang}/flyme2themoon_web.json', site / f'i18n/{lang}/flyme2themoon_web.json') for lang in ['en', 'zh', 'ja']]
    # Preserve overwritten files in the game workspace before touching the website.
    backup = ROOT / 'out/web/backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    if (target / 'runtime').is_dir():
        shutil.copytree(target / 'runtime', backup / 'projects/FlyMe2TheMoon/web/runtime')
    if (target / 'BUILD.md').is_file():
        previous = backup / 'projects/FlyMe2TheMoon/web/BUILD.md'
        previous.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(target / 'BUILD.md', previous)
    for source, destination in mappings:
        if destination.is_file():
            previous = backup / destination.relative_to(site)
            previous.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(destination, previous)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    shutil.copytree(runtime, target / 'runtime', dirs_exist_ok=True)
    shutil.copy2(ROOT / 'docs/development/web-build.md', target / 'BUILD.md')
    print('Prepared website at', target)
    print('Previous deployment retained at', backup)


if __name__ == '__main__':
    main()
