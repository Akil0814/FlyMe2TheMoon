"""Build the current 640x960 portrait browser edition; never commits or pushes."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
LIMIT = 32 * 1024 * 1024


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emsdk', type=Path, default=Path(os.environ.get('EMSDK', r'G:\DevelopTools\Toolchains\emsdk')))
    parser.add_argument('--cmake', default=r'G:\DevelopTools\BuildTools\CMake\bin\cmake.exe')
    parser.add_argument('--ninja', default=r'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--package-only', action='store_true')
    parser.add_argument('--fresh', action='store_true', help='Discard cached CMake feature checks')
    args = parser.parse_args()
    sdk = args.emsdk.resolve(strict=True)
    em = sdk / 'upstream/emscripten'
    version = (em / 'emscripten-version.txt').read_text().strip().strip('"')
    game_version = re.search(r'project\(FlyMe2TheMoon\s+VERSION\s+(\d+\.\d+\.\d+)',
                             (ROOT / 'CMakeLists.txt').read_text(encoding='utf8'))[1]
    if version != '5.0.6':
        raise RuntimeError('This build is pinned to Emscripten 5.0.6, found ' + version)
    env = os.environ.copy()
    env['EM_CONFIG'] = str(sdk / '.emscripten')
    env['EMSDK_PYTHON'] = sys.executable
    temporary = ROOT / 'out/web-temp'
    temporary.mkdir(parents=True, exist_ok=True)
    env['TEMP'] = env['TMP'] = str(temporary)
    # Keep caches and all generated files in this workspace.
    env['EM_CACHE'] = str(ROOT / 'out/web-cache')
    env['PATH'] = os.pathsep.join([str(Path(args.cmake).parent), str(Path(args.ninja).parent),
                                  str(em), env.get('PATH', '')])
    build = ROOT / 'out/build/web-portrait'
    def run(command, **kwargs):
        subprocess.run([str(x) for x in command], cwd=ROOT, env=env, check=True, **kwargs)
    if not args.package_only:
        run([sys.executable, em / 'emcmake.py', args.cmake, *(['--fresh'] if args.fresh else []), '-S', ROOT, '-B', build,
             '-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=' + args.ninja,
             '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
             '-DELYSIA_ENABLE_IMGUI=OFF', '-DELYSIA_ENABLE_ASAN=OFF'])
        run([args.cmake, '--build', build, '--target', 'FlyMe2TheMoon', '-j', args.jobs])
    output = ROOT / 'out/web-portrait'
    runtime = output / 'runtime'
    runtime.mkdir(parents=True, exist_ok=True)
    for name in ['FlyMe2TheMoon.js', 'FlyMe2TheMoon.wasm']:
        shutil.copy2(build / name, runtime / name)
    files = sorted(p for p in (ROOT / 'assets').rglob('*') if p.is_file())
    chunks, chunk, size = [], [], 0
    for path in files:
        length = path.stat().st_size
        if length > LIMIT:
            raise RuntimeError('Single asset exceeds chunk limit: ' + str(path))
        if chunk and size + length > LIMIT:
            chunks.append(chunk)
            chunk, size = [], 0
        chunk.append(path)
        size += length
    if chunk:
        chunks.append(chunk)
    packages = []
    for index, paths in enumerate(chunks):
        name = f'assets-{index:02d}'
        # Response file avoids Windows command line limits for large asset lists.
        response = output / f'{name}.rsp'
        arguments = ['--preload'] + [p.relative_to(ROOT).as_posix() + '@/' + p.relative_to(ROOT).as_posix() for p in paths]
        response.write_text('\n'.join(json.dumps(x) for x in arguments), encoding='utf8')
        run([sys.executable, em / 'tools/file_packager.py', runtime / (name + '.data'),
             '@' + str(response), '--js-output=' + str(runtime / (name + '.js')), '--no-node', '--quiet'])
        packages.append({'script': name + '.js', 'data': name + '.data',
                         'bytes': (runtime / (name + '.data')).stat().st_size})
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    asset_manifest = {p.relative_to(ROOT).as_posix(): {'bytes': p.stat().st_size, 'sha256': sha(p)} for p in files}
    (runtime / 'asset-manifest.json').write_text(json.dumps(asset_manifest, indent=2) + '\n', encoding='utf8')
    managed = ['FlyMe2TheMoon.js', 'FlyMe2TheMoon.wasm', 'asset-manifest.json']
    managed += [name for package in packages for name in [package['script'], package['data']]]
    entries = {name: {'bytes': (runtime / name).stat().st_size, 'sha256': sha(runtime / name)} for name in managed}
    build_id = hashlib.sha256(json.dumps(entries, sort_keys=True).encode()).hexdigest()[:16]
    manifest = {'version': 1, 'gameVersion': game_version, 'displayMode': 'portrait',
                'logicalWidth': 640, 'logicalHeight': 960, 'buildId': build_id, 'emscripten': version, 'sourceRevision': source,
                'sourceHasLocalChanges': dirty, 'runtime': 'FlyMe2TheMoon.js', 'packages': packages,
                'downloadBytes': sum(v['bytes'] for k, v in entries.items() if k != 'asset-manifest.json'),
                'assetBytes': sum(p.stat().st_size for p in files), 'assetCount': len(files), 'files': entries}
    (runtime / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf8')
    notices = runtime / 'licenses'
    for base in [ROOT / 'thirdparty', ROOT / 'game/vendor']:
        for path in base.rglob('*'):
            if path.is_file() and (path.name.lower().startswith(('license', 'licence', 'copying', 'copyright', 'notice')) or path.name.lower() in ['ofl.txt', 'ftl.txt']):
                target = notices / path.relative_to(ROOT)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
    for name in ['LICENSE', 'THIRD_PARTY_NOTICES.md']:
        shutil.copy2(ROOT / name, runtime / name)
    lua_header = (ROOT / 'game/vendor/lua/lua.h').read_text(encoding='utf8')
    lua_notice = lua_header.rsplit('/******************************************************************************', 1)[1]
    lua_notice = lua_notice.split('******************************************************************************/', 1)[0]
    lua_notice = '\n'.join(line.lstrip('* ').rstrip() for line in lua_notice.splitlines()).strip()
    (notices / 'lua.txt').write_text(lua_notice + '\n', encoding='utf8')
    mit_terms = lua_notice[lua_notice.index('Permission is hereby granted'):]
    (notices / 'nlohmann-json.txt').write_text(
        'JSON for Modern C++ 3.12.0\nCopyright (c) 2013-2025 Niels Lohmann\n'
        'https://github.com/nlohmann/json\nMIT License\n\n' + mit_terms + '\n', encoding='utf8')
    print(f'Web build {build_id}: {len(files)} assets, {len(packages)} packages, {manifest["downloadBytes"] / 1048576:.1f} MiB')
    print('Prepared at', output)


if __name__ == '__main__':
    main()
