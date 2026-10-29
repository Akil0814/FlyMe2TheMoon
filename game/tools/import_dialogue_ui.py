"""Copy the original HD dialogue plate and shared Skip states unchanged."""
import shutil
from asset_usage import assets_root, read, write
from analyze_reference import REF


def main():
    assets = assets_root()
    sprites = REF / "08_PNG解析/03_拆分帧"
    sources = {"fly.InGameUI.tutorialPanelHalf": (
        sprites / "InGameUI/InGameUI-hd/tutorialPanelHalf.png",
        "InGameUI/tutorialPanelHalf.png")}
    # All three language atlases contain byte-identical Skip artwork per state.
    for state, original in [("up", "Up"), ("down", "Down")]:
        source = sprites / f"TextImages/TextImages-hd/inGame/skipBtn{original}.png"
        for atlas in ["TextImages_chs-hd", "TextImages_jpn-hd"]:
            assert source.read_bytes() == (sprites / f"TextImages/{atlas}/inGame/skipBtn{original}.png").read_bytes()
        name = f"dialogue_skip_{state}"
        sources[f"fly.frontend.{name}"] = (source, f"frontend/{name}.png")
    manifest_path = assets / "configs/manifests/textures_manifest.json"
    manifest = read(manifest_path)
    for key, (source, relative) in sources.items():
        target = assets / "textures" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        manifest["textures"][key] = {"path": relative}
    write(manifest_path, manifest)
    print(f"Imported {len(sources)} original HD dialogue textures")


if __name__ == "__main__":
    main()
