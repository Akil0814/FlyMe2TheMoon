"""Read original TutorialDiagram lookup tables without executing the reference."""
import json
import shutil
from inspect_original import Original
from analyze_reference import ROOT


def main():
    binary = Original()
    frames = []
    for index in range(80):
        frames.append({
            'up': binary.unpack('<2f', 0x1f3550 + index * 8),
            'down': binary.unpack('<2f', 0x1f37d0 + index * 8),
            'blank': binary.unpack('<2f', 0x1f3a50 + index * 8),
            'arrow': binary.unpack('<3f', 0x1ac9e8 + index * 12),
            'doll': binary.data[binary.offset(0x1f3500) + index],
        })
    target = ROOT / 'assets/configs/fly/tutorial_visual.json'
    target.write_text(json.dumps({'fps': 10, 'direction_frames': frames}, indent=2) + '\n', encoding='utf8')
    manifest_path=ROOT/'assets/configs/manifests/textures_manifest.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf8'))
    manifest['textures'].pop('fly.frontend.tutorial_demo_zh-Hans',None)
    for language,folder in [('en','TextImages-hd'),('zh-Hans','TextImages_chs-hd'),('ja','TextImages_jpn-hd')]:
        source=ROOT/'reference/Fly2theMoon_1.1_ios_3.2/08_PNG解析/03_拆分帧/TextImages'/folder/'inGame/blackEdge.png'
        destination=ROOT/'assets/textures/fly/frontend'/f'tutorial_demo_{language}.png'
        shutil.copyfile(source,destination)
        key='zh' if language=='zh-Hans' else language
        manifest['textures'][f'fly.frontend.tutorial_demo_{key}']={'path':f'fly/frontend/tutorial_demo_{language}.png'}
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(target)


if __name__ == '__main__':
    main()
