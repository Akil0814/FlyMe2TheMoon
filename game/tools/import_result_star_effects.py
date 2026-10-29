"""Copy SuccessLayer.createStarParticle's original standalone grayscale texture."""
import json
import shutil
from analyze_reference import original_file
from asset_usage import assets_root


def main():
    assets = assets_root()
    shutil.copyfile(original_file('stars2-grayscale.png'), assets / 'textures/result_star_particles.png')
    manifest = assets / 'configs/manifests/textures_manifest.json'
    data = json.loads(manifest.read_text(encoding='utf8'))
    data['textures']['fly.result_star_particles'] = {'path': 'result_star_particles.png'}
    manifest.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


if __name__ == '__main__':
    main()
