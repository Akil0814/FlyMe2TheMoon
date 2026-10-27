"""Audit shipped assets against game code, level data and dynamic resource families.

Resource IDs are stable; paths are relative to their resource-type directory.
Unknown families are retained for review. Registration alone is not a usage root.
Importers share this policy so they cannot recreate intentionally pruned assets.
"""
import argparse
from collections import defaultdict
import hashlib
import json
import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]


def assets_root():
    return Path(os.environ.get("FLY_ASSETS_ROOT", ROOT / "assets")).resolve()


ROLE_ACTIONS = ("charIdle", "charUp", "charFree", "charLeft", "charRight", "charDown",
                "charDeath", "charStand", "charLookLeftRight", "charLookLeft", "charLookRight")
ROLE_EFFECTS = ("ball", "ballDisapear", "ballExplode")
STATE_KINDS = {8, 24, 25, 101, *range(105, 111), *range(112, 117)}
KNOWN_TEXTURE_FAMILIES = ("fly.terrain", "fly.far", "fly.CharActions.",
                          "fly.DynamicObjectsLite.", "fly.frontend.", "fly.ui.",
                          "fly.InGameUI.", "fly.TutorialUI.", "fly.Options.",
                          "fly.Panel.", "fly.LevelSelect.", "fly.background.")


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf8")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def usage(assets):
    """Expand runtime IDs, including branches not exercised by a replay."""
    sources = [p for p in (ROOT / "game").rglob("*")
               if p.suffix in {".cpp", ".h"} and not {"tests", "vendor", "testing"}.intersection(p.parts)]
    source = "\n".join(p.read_text(encoding="utf8") for p in sources)
    tokens = set(re.findall(r'"([^"\n]*)"', source))
    configs = assets / "configs"
    levels = [read(configs / "levels" / f"{i:03}.json") for i in range(1, 61)]
    catalog = read(configs / "catalog.json")
    textures, animations, sounds, music = (defaultdict(list) for _ in range(4))

    def keep(table, key, reason):
        if reason not in table[key]:
            table[key].append(reason)

    for token in tokens:
        if token.startswith("fly."):
            for table in [textures, animations, sounds, music]:
                keep(table, token, "literal game-code resource ID")
    entities = {}
    for level in levels:
        for decor in level["decor"]:
            keep(textures, decor["key"], f"level {level['id']:03} decoration")
        for entity in level["entities"]:
            entities[entity["key"]] = entity
    manifests = assets / "configs/manifests"
    texture_keys = read(manifests / "textures_manifest.json")["textures"]
    animation_entries = read(manifests / "animations_manifest.json")["animations"]
    for root, entity in entities.items():
        # Keep the complete state/component family, not just its currently visible frame.
        for key in texture_keys:
            if key == root or key.startswith(root + "."):
                keep(textures, key, "60-level entity state/component family")
        for entry in animation_entries:
            key = entry["key"]
            if key == root or key.startswith(root + "."):
                keep(animations, key, "60-level entity atlas/component family")
    for action in ROLE_ACTIONS:
        keep(animations, "fly.CharActions." + action, "Player constructor animation pool")
    for effect in ROLE_EFFECTS:
        keep(animations, "fly.CharActions.charEffects." + effect, "Player transition frame pool")
    for key in texture_keys:
        for family in ["fly.ui.", "fly.TutorialUI.", "fly.frontend."]:
            if key.startswith(family) and key[len(family):] in tokens:
                keep(textures, key, "dynamic UI/portrait/tutorial name in game code")
    for name in ["menu", "moon", "settings"]:
        keep(textures, "fly.frontend.background_" + name, "MoonBackdrop scene argument")
    for color in "PYOG":
        for prefix in ["point", "lock", "starDown", "starUp", "starGrey"]:
            keep(textures, "fly.frontend." + prefix + color, "all chapter colors and unlock/star states")
    for lang in ["en", "ja", "zh"]:
        keep(textures, "fly.frontend.tutorial_demo_" + lang, "tutorial language fallback including ko/zh-Hant")
    descriptions = read(configs / "text/en/gcDescriptions.json")["achievements"]
    for achievement in descriptions:
        keep(textures, "fly.frontend.achv" + achievement, "achievement album, earned and unearned states")
    scripts = "\n".join(level["script"] for level in levels)
    for face in re.findall(r"\bface[A-Z]\w*", source + "\n" + scripts):
        keep(textures, "fly.ui." + face, "Lua face constants / dialogue portrait")
    for filename in re.findall(r'"(\d+-\d+)"', source + "\n" + scripts):
        keep(sounds, "fly.voice_" + filename.replace("-", "_"), "game audio pool or original Lua sound.play")
    for n in range(1, 11):
        keep(sounds, f"fly.voice_90_{n}", "CrystalSequence dynamically clamped to 1..10")
    for n in range(1, 4):
        keep(sounds, f"fly.voice_106_{n}", "result star reveal dynamically emits 106-1..3")
    # Engine UI defaults may use these independently of the game's GlassButton.
    for key in ["system.button_click_down", "system.button_click_up"]:
        keep(sounds, key, "engine UI default sounds")
    return {"textures": textures, "animations": animations, "sounds": sounds, "music": music}, levels, catalog


def audit(assets):
    tables, levels, catalog = usage(assets)
    manifests = assets / "configs/manifests"
    entries = {"textures": read(manifests / "textures_manifest.json")["textures"],
               "animations": {e["key"]: e for e in read(manifests / "animations_manifest.json")["animations"]}}
    audio = read(manifests / "audio_manifest.json")
    entries.update(sounds=audio["sounds"], music=audio["music"])
    retained, removed, errors = [], [], []
    files = defaultdict(list)
    for kind, group in entries.items():
        for key, entry in group.items():
            reasons = tables[kind].get(key, [])
            known = key.startswith(KNOWN_TEXTURE_FAMILIES) or key in {"fly.role_fire", "fly.laser_stars"}
            if not reasons and not (known if kind == "textures" else key.startswith("fly.")):
                reasons = ["uncertain: unclassified resource family retained"]
            base = assets / ("audio" if kind in {"sounds", "music"} else "textures")
            if kind == "animations":
                # The loader expects the numbered sequence; never deduplicate its files.
                paths = [base / entry["path"] / f"{entry['frame_prefix']}_{n:03}.png"
                         for n in range(entry["frame_count"])]
            else:
                paths = [base / entry["path"]]
            row = {"kind": kind, "key": key, "files": [p.relative_to(assets).as_posix() for p in paths],
                   "reasons": reasons or ["no usage after 60-level, code and dynamic-family expansion"]}
            (retained if reasons else removed).append(row)
            for path in paths:
                files[path].append((bool(reasons), row))
                if not path.is_file():
                    errors.append("Missing manifest file: " + str(path))
    # Mandatory dynamic lookups must exist; optional literal probes may intentionally be absent.
    required_textures = {d["key"] for level in levels for d in level["decor"]}
    required_animations = {"fly.CharActions." + action for action in ROLE_ACTIONS}
    required_animations |= {"fly.CharActions.charEffects." + effect for effect in ROLE_EFFECTS}
    components = {**{kind: ("S01",) for kind in range(105, 111)},
                  112: ("S01", "S02"), 113: ("S01",),
                  114: ("S01", "S02", "S03", "S04"), 115: ("S01", "S02"),
                  116: ("S01", "S02", "S03", "S04")}
    for level in levels:
        for entity in level["entities"]:
            if entity["item"] or entity["kind"] in STATE_KINDS:
                required_textures.add(entity["key"] + ".first")
                required_animations.add(entity["key"])
            else:
                required_textures.add(entity["key"])
            for suffix in components.get(entity["kind"], ()):
                required_animations.add(entity["key"] + "." + suffix)
    for key in required_textures:
        if key not in entries["textures"]:
            errors.append("Missing dynamically required texture: " + key)
    for kind, table in tables.items():
        for key, reasons in table.items():
            # Complete dynamic pools are strict, unlike generic literal detection.
            if any(reason.startswith(("all chapter", "tutorial language", "achievement album", "Lua face", "CrystalSequence", "result star")) for reason in reasons):
                if key not in entries[kind]:
                    errors.append("Missing dynamically required " + kind + ": " + key)
    for key in required_animations:
        if key not in entries["animations"]:
            errors.append("Missing player/entity atlas: " + key)
    for key in tables["sounds"]:
        if re.fullmatch(r"fly\.voice_\d+_\d+", key) and key not in entries["sounds"]:
            errors.append("Missing Lua/game audio pool member: " + key)
    for key in ["fly.SkyTunnel", "fly.MoonTrip", "fly.SymphonyofRush", "fly.LifeforSpeed",
                "fly.Hectic", "fly.SpiralofAdventure"]:
        if key not in entries["music"]:
            errors.append("Missing scene/level music: " + key)
    for folder in [assets / "textures/fly", assets / "audio/fly", assets / "configs/fly"]:
        if folder.exists():
            errors.append("Legacy directory exists: " + str(folder))
    for path in (ROOT / "game").rglob("*"):
        if path.suffix not in {".cpp", ".h", ".py", ".ps1", ".cmd"} or "vendor" in path.parts:
            continue
        source = path.read_text(encoding="utf8")
        for kind in ["configs", "textures", "audio"]:
            if f"assets/{kind}/fly/" in source:
                errors.append("Legacy path in active source: " + path.relative_to(ROOT).as_posix())
    # Remaining config/i18n/fonts/engine/bootstrap files have independent loaders.
    protected = [p for p in assets.rglob("*") if p.is_file() and p not in files]
    font_manifest = read(manifests / "fonts_manifest.json")
    font_files = {entry["file"] for entry in font_manifest["fonts"]}
    unused_fonts = [p for p in (assets / "fonts").glob("*.ttf") if p.name not in font_files]
    for folder in ["textures", "audio"]:
        for path in (assets / folder).rglob("*"):
            if path.is_file() and path not in files:
                errors.append("Unregistered runtime file: " + path.relative_to(assets).as_posix())
    for file in font_files:
        if not (assets / "fonts" / file).is_file():
            errors.append("Missing font: " + file)
    locales = read(manifests / "i18n_manifest.json")
    for language in locales["languages"]:
        for filename in locales["file"]:
            if not (assets / "i18n" / language / filename).is_file():
                errors.append("Missing localization: " + language + "/" + filename)
    for language in ["en", "ja", "zh-Hans"]:
        for filename in ["strings.json", "gcDescriptions.json"]:
            path = assets / "configs/text" / language / filename
            if not path.is_file():
                errors.append("Missing original story/achievement localization: " + str(path))
            else:
                read(path)
    return {"retained": retained, "unused_entries": removed, "errors": errors,
            "unused_files": [p.relative_to(assets).as_posix() for p, refs in files.items() if not any(used for used, _ in refs)]
                            + [p.relative_to(assets).as_posix() for p in unused_fonts],
            "unused_font_reason": "not declared in the font manifest; game font resolver uses manifest keys and engine fallback fonts",
            "protected_files": [p.relative_to(assets).as_posix() for p in protected if p not in unused_fonts]}, entries


def prune_imported(assets):
    """Apply the same reachability policy after a complete import, on an explicit tree."""
    report, _ = audit(assets)
    if report["errors"]:
        raise RuntimeError("Cannot prune invalid assets: " + "; ".join(report["errors"][:5]))
    removed = defaultdict(set)
    for row in report["unused_entries"]:
        removed[row["kind"]].add(row["key"])
    manifests = assets / "configs/manifests"
    for name in ["textures", "animations", "audio"]:
        path = manifests / (name + "_manifest.json")
        data = read(path)
        if name == "animations":
            data["animations"] = [e for e in data["animations"] if e["key"] not in removed["animations"]]
        else:
            for group in (["sounds", "music"] if name == "audio" else ["textures"]):
                data[group] = {key: entry for key, entry in data[group].items() if key not in removed[group]}
        write(path, data)
    asset_root = assets.resolve()
    for relative in report["unused_files"]:
        path = (assets / relative).resolve()
        if not path.is_relative_to(asset_root):
            raise ValueError("Resource escapes assets: " + relative)
        path.unlink()
    for path in sorted(assets.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if path.is_dir() and not any(path.iterdir()):
            path.rmdir()
    return report


def high_resolution_audit(assets):
    """Require an exact HD source match, or a native source with no HD alternate."""
    reference = ROOT / "reference/Fly2theMoon_1.1_ios_3.2/08_PNG解析"
    by_hash = defaultdict(list)
    hd_names = set()

    def is_hd(path):
        return any("-hd" in part or "@2x" in part for part in path.parts)

    def normalized(path):
        return path.relative_to(reference).as_posix().replace("-hd", "").replace("@2x", "")

    for path in reference.rglob("*.png"):
        by_hash[digest(path)].append(path)
        if is_hd(path):
            hd_names.add(normalized(path))
    supplied = defaultdict(list)
    for path in (ROOT / "reference/generate").glob("*.png"):
        supplied[digest(path)].append(path)
    rows, errors = [], []
    custom = {entry["path"]: entry for entry in read(ROOT / "game/tools/custom_hd_assets.json")}
    for path in sorted((assets / "textures").rglob("*.png")):
        sha = digest(path)
        originals = by_hash.get(sha, [])
        hd = [p for p in originals if is_hd(p)]
        if hd:
            status, source = "original-hd", hd[0]
        elif originals and not any(normalized(p) in hd_names for p in originals):
            status, source = "original-native-no-hd-alternative", originals[0]
        elif supplied.get(sha):
            status, source = "supplied-native-resolution", supplied[sha][0]
        elif custom.get(path.relative_to(assets).as_posix(), {}).get("sha256") == sha:
            status, source = "project-custom-hd-native", None
        else:
            status, source = "unverified", None
            errors.append(path.relative_to(assets).as_posix())
        # PNG IHDR dimensions; this check needs no imaging package.
        raw = path.read_bytes()
        rows.append({"path": path.relative_to(assets).as_posix(), "sha256": sha,
                     "width": int.from_bytes(raw[16:20], "big"), "height": int.from_bytes(raw[20:24], "big"),
                     "status": status, "source": source.relative_to(ROOT).as_posix() if source else None})
    return {"files": rows, "errors": errors,
            "engine_and_preload": "preserved byte-for-byte; engine native assets and startup artwork have independent loading paths"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=ROOT / "assets")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--prune", action="store_true")
    parser.add_argument("--check-hd", action="store_true")
    args = parser.parse_args()
    report = prune_imported(args.assets) if args.prune else audit(args.assets)[0]
    if args.check_hd:
        report["high_resolution"] = high_resolution_audit(args.assets)
        report["errors"].extend("Unverified resolution: " + path for path in report["high_resolution"]["errors"])
    if not args.prune and report["unused_files"]:
        report["errors"].append("Unused assets remain; inspect --report before using --prune")
    if args.report:
        write(args.report, report)
    print(json.dumps({"retained_entries": len(report["retained"]), "unused_entries": len(report["unused_entries"]),
                      "unused_files": len(report["unused_files"]), "errors": report["errors"][:10]}))
    raise SystemExit(bool(report["errors"]))


if __name__ == "__main__":
    main()
