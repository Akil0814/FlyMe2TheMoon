"""Run existing input replays in an isolated, standalone copy of the game.

Evidence stays under out/qa; neither player saves nor historical evidence is used.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = ["role-transition-moon", "role-transition-portal", "audio-level9-laser",
             "audio-level24-homing", "audio-level31-bomb", "audio-level39-pause"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", default="x64-Debug")
    parser.add_argument("--label", required=True)
    parser.add_argument("--scenarios", nargs="+", default=SCENARIOS)
    args = parser.parse_args()
    evidence = ROOT / "out/qa/assets-cleanup" / args.label
    evidence.mkdir(parents=True, exist_ok=False)
    stage = Path(tempfile.mkdtemp(prefix="FlyMe2TheMoon-assets-"))
    (evidence / "stage.txt").write_text(str(stage), encoding="utf8")
    shutil.copytree(ROOT / "assets", stage / "assets")
    build = ROOT / "out/build" / args.build
    for name in ["FlyMe2TheMoon.exe", "SDL3.dll"]:
        shutil.copy2(build / name, stage / name)
    for path in (ROOT / "out/qa").glob("*-preview.bmp"):
        shutil.copy2(path, evidence / path.name)
    output = stage / "docs/development/test-evidence"
    output.mkdir(parents=True)
    env = dict(os.environ, SDL_AUDIODRIVER="dummy")
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    results = []
    print("Standalone assets staged:", stage, flush=True)
    for name in args.scenarios:
        scenario = ROOT / "game/testing" / (name + ".json")
        data = json.loads(scenario.read_text(encoding="utf8"))
        record_name = data["name"]
        env["FLY_REPLAY_SCENARIO"] = str(scenario)
        try:
            with (evidence / (record_name + ".log")).open("w", encoding="utf8") as log:
                run = subprocess.run([str(stage / "FlyMe2TheMoon.exe")], cwd=stage,
                                     env=env, stdout=log, stderr=subprocess.STDOUT,
                                     startupinfo=startup, timeout=data["duration"] + 65)
            report = output / (record_name + ".json")
            result = json.loads(report.read_text(encoding="utf8")) if report.exists() else {}
            row = {"scenario": name, "exit_code": run.returncode,
                   "passed": run.returncode == 0 and result.get("passed", False)}
        except subprocess.TimeoutExpired:
            row = {"scenario": name, "passed": False, "error": "timeout"}
        for path in output.glob(record_name + ".*"):
            shutil.copy2(path, evidence / path.name)
        results.append(row)
        (evidence / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf8")
        print(row, flush=True)
    # Preserve the standalone directory for inspection; its path is recorded above.
    raise SystemExit(0 if all(row["passed"] for row in results) else 1)


if __name__ == "__main__":
    main()
