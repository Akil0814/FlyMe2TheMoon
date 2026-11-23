"""Validate, back up and stage both browser editions. Never commits or pushes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]
EDITIONS = [('web', 'runtime', 'landscape', 1280, 720),
            ('web-portrait', 'runtime-portrait', 'portrait', 640, 960)]


def hashes(directory):
    return {p.relative_to(directory).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(directory.rglob('*')) if p.is_file()}


def validate(runtime, mode, width, height, version):
    manifest = json.loads((runtime / 'manifest.json').read_text(encoding='utf8'))
    if (manifest.get('version'), manifest.get('gameVersion'), manifest.get('displayMode'),
        manifest.get('logicalWidth'), manifest.get('logicalHeight')) != (1, version, mode, width, height):
        raise RuntimeError('Package version or presentation mismatch: ' + str(runtime))
    if manifest.get('sourceHasLocalChanges') is not False or not re.fullmatch(r'[0-9a-f]{40}', manifest.get('sourceRevision', '')):
        raise RuntimeError('Release packages must record a clean source commit')
    for name, entry in manifest['files'].items():
        path = runtime / name
        if not path.resolve().is_relative_to(runtime.resolve()):
            raise RuntimeError('Package path leaves runtime directory')
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256'] or path.stat().st_size != entry['bytes']:
            raise RuntimeError('Package integrity check failed: ' + str(path))
    for package in manifest['packages']:
        if not 0 < package['bytes'] <= 32 * 1024 * 1024:
            raise RuntimeError('Data package exceeds 32 MiB')
        if manifest['files'][package['data']]['bytes'] != package['bytes']:
            raise RuntimeError('Package size does not match manifest')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-root', type=Path, default=Path(r'G:\Coding\Projects\Akil0814.github.io'))
    args = parser.parse_args()
    site = args.site_root.resolve(strict=True)
    target = site / 'projects/FlyMe2TheMoon/web'
    if not (site / 'projects/FlyMe2TheMoon/FlyMe2TheMoon.html').is_file():
        raise RuntimeError('Expected existing FlyMe2TheMoon website')
    version = re.search(r'project\(FlyMe2TheMoon\s+VERSION\s+(\d+\.\d+\.\d+)',
                        (ROOT / 'CMakeLists.txt').read_text(encoding='utf8'))[1]
    manifests = {}
    for edition, directory, mode, width, height in EDITIONS:
        manifests[directory] = validate(ROOT / 'out' / edition / 'runtime', mode, width, height, version)
    page = ROOT / 'game/platform/web'
    mappings = [(page / name, target / name) for name in
                ['index.html', 'flyme2themoon_web.css', 'flyme2themoon_web.js', 'platform.js', '.gitattributes']]
    mappings += [(page / f'i18n/{lang}/flyme2themoon_web.json', site / f'i18n/{lang}/flyme2themoon_web.json') for lang in ['en', 'zh', 'ja']]
    mappings += [(ROOT / 'docs/development/web-build.md', target / 'BUILD.md')]
    for source, _ in mappings:
        if not source.is_file():
            raise RuntimeError('Managed page source is absent: ' + str(source))
    original = {directory: hashes(target / directory) for _, directory, *_ in EDITIONS}
    backup = ROOT / 'out/web-release/backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    for _, directory, *_ in EDITIONS:
        if (target / directory).is_dir():
            shutil.copytree(target / directory, backup / 'projects/FlyMe2TheMoon/web' / directory)
    for _, destination in mappings:
        if destination.is_file():
            previous = backup / destination.relative_to(site)
            previous.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(destination, previous)
    backup.mkdir(parents=True, exist_ok=True)
    (backup / 'runtime-hashes.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf8')
    for source, destination in mappings:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    for edition, directory, mode, width, height in EDITIONS:
        destination = target / directory
        previous = destination / 'manifest.json'
        old_files = json.loads(previous.read_text(encoding='utf8'))['files'] if previous.is_file() else {}
        shutil.copytree(ROOT / 'out' / edition / 'runtime', destination, dirs_exist_ok=True)
        for name in old_files.keys() - manifests[directory]['files'].keys():
            obsolete = destination / name
            if not obsolete.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError('Old package path leaves runtime directory')
            obsolete.unlink(missing_ok=True)
        validate(destination, mode, width, height, version)
        print(f'{mode}: v{version}, build {manifests[directory]["buildId"]}')
    print('Staged dual-version page at', target)
    print('Backup:', backup)


if __name__ == '__main__':
    main()
