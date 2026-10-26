"""Stage the Release/ImGui build and its runtime assets, then publish release/."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def add_unlocked_save(stage):
    progress = {'version': 1, 'stars': [0] * 60, 'scores': [0] * 60,
                'score_verified': [False] * 60, 'legacy_scores': [0] * 60,
                'deaths': [0] * 60, 'best_time': [0.0] * 60, 'crystals': 0,
                'achievements': [], 'story_flags': [], 'no_pickup_clear': False}
    dependencies = json.loads((stage / 'assets/configs/fly/catalog.json').read_text(
        encoding='utf8'))['dependencies']
    for level in range(1, 61):
        if not all(progress['stars'][dependency - 1] >= 0
                   for dependency in dependencies[str(level)]):
            raise RuntimeError('Generated test save leaves level locked: ' + str(level))
    save = {'format_version': 1, 'types': {'game.progress': 'string'},
            'values': {'game.progress': json.dumps(progress, separators=(',', ':'))}}
    folder = stage / '关卡全开存档'
    folder.mkdir()
    (folder / 'fly_progress.json').write_text(json.dumps(save, indent=2) + '\n', encoding='utf8')
    (folder / '使用说明.txt').write_text(
        '全 60 关解锁测试存档\n\n'
        '1. 先退出游戏。\n'
        '2. 备份 release\\player_data\\saves 中原有的 fly_progress.json 及其 .bak。\n'
        '3. 将本文件夹中的 fly_progress.json 复制到游戏旁边的 player_data\\saves\\。\n'
        '   如果尚未运行过游戏，可自行创建 player_data\\saves 文件夹。\n'
        '4. 重新启动 FlyMe2TheMoon.exe，即可选择全部 60 关。\n\n'
        '当前解锁规则依赖前置关卡通关状态，因此此档将 60 关设为零星通关。\n'
        '星数、分数、水晶、死亡和最佳时间均为零，未额外填写成就或剧情已读标记。\n'
        '此档需手动复制后才生效，直接启动程序仍使用自己的正常存档。\n', encoding='utf-8-sig')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-dir', type=Path, required=True,
                        help='MSVC x64 Microsoft.VC14x.CRT redistributable directory')
    args = parser.parse_args()
    build = ROOT / 'out/build/release'
    cache = (build / 'CMakeCache.txt').read_text(encoding='utf8')
    for required in ['CMAKE_BUILD_TYPE:STRING=Release', 'ELYSIA_ENABLE_IMGUI:BOOL=ON',
                     'ELYSIA_ENABLE_ASAN:BOOL=OFF']:
        if required not in cache.splitlines():
            raise RuntimeError('Unexpected build configuration: missing ' + required)
    runtime = args.runtime_dir.resolve(strict=True)
    if not all((runtime / name).is_file() for name in ['vcruntime140.dll', 'msvcp140.dll']):
        raise RuntimeError('The x64 VC redistributable directory is incomplete')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    stage = ROOT / 'out/packages' / ('release-stage-' + stamp)
    stage.mkdir(parents=True)
    for name in ['FlyMe2TheMoon.exe', 'SDL3.dll']:
        shutil.copy2(build / name, stage / name)
    for path in runtime.glob('*.dll'):
        shutil.copy2(path, stage / path.name)
    shutil.copytree(ROOT / 'assets', stage / 'assets')
    add_unlocked_save(stage)

    # Preserve notices for bundled code, codecs and fonts; do not copy sources.
    notices = stage / 'licenses'
    candidates = list((ROOT / 'thirdparty').rglob('*'))
    candidates += list((ROOT / 'game/vendor').rglob('*'))
    candidates += list(ROOT.glob('LICENSE*'))
    for path in candidates:
        name = path.name.lower()
        if path.is_file() and (name.startswith(('license', 'licence', 'copying', 'copyright', 'notice'))
                               or name in ['ofl.txt', 'ftl.txt']):
            target = notices / path.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    lua_header = (ROOT / 'game/vendor/lua/lua.h').read_text(encoding='utf8')
    lua_notice = lua_header.rsplit('/******************************************************************************', 1)[1]
    lua_notice = lua_notice.split('******************************************************************************/', 1)[0]
    lua_notice = '\n'.join(line.lstrip('* ').rstrip() for line in lua_notice.splitlines()).strip()
    (notices / 'lua.txt').write_text(lua_notice + '\n', encoding='utf8')
    mit_terms = lua_notice[lua_notice.index('Permission is hereby granted'):]
    (notices / 'nlohmann-json.txt').write_text(
        'JSON for Modern C++ 3.12.0\nCopyright (c) 2013-2025 Niels Lohmann\n'
        'https://github.com/nlohmann/json\nMIT License\n\n' + mit_terms + '\n', encoding='utf8')

    (stage / '启动说明.txt').write_text(
        'FlyMe2TheMoon — Windows x64 Release 测试构建\n\n'
        '双击 FlyMe2TheMoon.exe 启动，请保留 assets 文件夹及旁边的 DLL。\n'
        '若从命令行启动，请先切换到本文件所在目录。\n'
        '启动保留 ElysiaEngine Logo 与加载确认；按界面提示继续。\n\n'
        '开发面板已编译保留：F2 显示/隐藏 ImGui 面板。\n'
        '局内面板可调相机缩放，并提供调试绘制、FPS、暂停与时间控制。\n'
        '飞行：鼠标按住角色下方喷射；Space / W / 上方向键上升，A / D 转向。\n'
        'P 暂停，R 重试，Esc 按场景提示返回。\n\n'
        '运行环境：Windows 10/11 x64。已随包附带 SDL3 和 VC C++ 运行库。\n'
        '此包不复制开发工作区存档；首次运行从新进度开始。\n'
        '如需全关测试，请按“关卡全开存档”文件夹内的说明手动使用附带存档。\n'
        '运行后会在当前包内生成 player_data 和 logs。\n'
        '这是当前开发版本的运行包，不表示全部 60 关已完成通关验收。\n',
        encoding='utf-8-sig')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip()
    info = {'created_utc': stamp, 'configuration': 'Release', 'architecture': 'x64',
            'imgui': True, 'asan': False, 'source_revision': revision,
            'source_has_local_changes': bool(dirty),
            'executable_sha256': digest(stage / 'FlyMe2TheMoon.exe'),
            'icon_source_sha256': digest(ROOT / 'reference/icon.png'),
            'icon_ico_sha256': digest(ROOT / 'game/platform/windows/app.ico'),
            'asset_files': sum(p.is_file() for p in (stage / 'assets').rglob('*'))}
    (stage / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf8')
    manifest = {p.relative_to(stage).as_posix(): {'bytes': p.stat().st_size, 'sha256': digest(p)}
                for p in sorted(stage.rglob('*')) if p.is_file()}
    (stage / 'package-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)
                                               + '\n', encoding='utf8')
    destination = ROOT / 'release'
    # Only publish to the explicitly named project release directory. Preserve
    # any earlier package instead of recursively deleting user files or saves.
    if destination.exists():
        backup = ROOT / 'out/packages' / ('release-previous-' + stamp)
        destination.rename(backup)
        print('Previous package retained at', backup)
    stage.rename(destination)
    print('Packaged', len(manifest), 'files to', destination)


if __name__ == '__main__':
    main()
