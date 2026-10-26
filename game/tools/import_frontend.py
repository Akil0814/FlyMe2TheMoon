"""Copy supplied widescreen artwork and decoded original UI; never edit reference files."""
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'reference/Fly2theMoon_1.1_ios_3.2/08_PNG解析'

def main():
    manifest_path = ROOT / 'assets/configs/manifests/textures_manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    outpaint=ROOT/'assets/textures/fly/frontend/background_menu_outpaint.png'
    if outpaint.is_file():
        manifest['textures']['fly.frontend.background_menu_outpaint']={'path':'fly/frontend/background_menu_outpaint.png'}
    sources = {f'background_{name}': ROOT / f'reference/generate/{source}.png'
               for name, source in [('menu', 'kiana'), ('settings', 'kiana2'), ('moon', 'moon')]}
    badges = REFERENCE / '01_独立图片/02_图形资源/独立图片/成就'
    for source in badges.glob('achv*@2x.png'):
        sources[source.stem.replace('@2x', '')] = source
    menu = REFERENCE / '03_拆分帧/MainMenu/MainMenu-hd'
    for name in ['mainMenuBtnUp', 'mainMenuBtnDown']:
        sources[name] = menu / f'{name}.png'
    levels = REFERENCE / '03_拆分帧/LevelSelect/LevelSelect-hd/lvlSel'
    for name in ['levelBtnUp', 'levelBtnRing', 'levelBtnLock']:
        sources[name] = levels / f'{name}.png'
    for color in 'OGYP':
        for prefix in ['starUp', 'starGrey', 'point']:
            name = prefix + color
            sources[name] = levels / f'{name}.png'
    for name, source in sources.items():
        target = ROOT / f'assets/textures/fly/frontend/{name}.png'
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        manifest['textures'][f'fly.frontend.{name}'] = {'path': f'fly/frontend/{name}.png'}
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    translations = {
        'en': 'Album {}',
        'zh-Hans': '星册 {}',
        'zh-Hant': '星冊 {}',
        'ja': 'アルバム {}',
        'ko': '앨범 {}',
    }
    for language, values in translations.items():
        path = ROOT / f'assets/i18n/{language}/base.json'
        data = json.loads(path.read_text(encoding='utf-8'))
        for key in ['menu_tagline','menu_journey','achievements_intro','settings_intro']:
            data['fly'].pop(key,None)
        data['fly']['album_page']=values
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Imported {len(sources)} original / supplied frontend images')

if __name__ == '__main__':
    main()
