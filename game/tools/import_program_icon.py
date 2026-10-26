"""Encode the supplied PNG as a Windows multi-resolution icon (requires Pillow)."""
from pathlib import Path
from PIL import Image


def main():
    root = Path(__file__).resolve().parents[2]
    source = root / 'reference/icon.png'
    target = root / 'game/platform/windows/app.ico'
    target.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(source) as image:
        if image.width != image.height:
            raise ValueError('Program icon must be square; do not crop the supplied image')
        image.convert('RGBA').save(target, format='ICO',
            sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
    print(target)


if __name__ == '__main__':
    main()
