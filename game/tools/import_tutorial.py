"""Decode the original plist's variable-length input records without altering it."""
import json
import math
import plistlib
import struct
from analyze_reference import ROOT, original_file


def decode(raw):
    header, count = struct.unpack_from('<fI', raw)
    cursor = 8
    frames = []
    for _ in range(count):
        flag = raw[cursor]
        cursor += 1
        if flag == 0:
            frames.append(None)
        elif flag == 1:
            x, y = struct.unpack_from('<ff', raw, cursor)
            cursor += 8
            assert math.isfinite(x) and math.isfinite(y)
            frames.append([x, y])
        else:
            raise ValueError(f'Unknown input flag {flag}')
    assert cursor == len(raw), (cursor, len(raw))
    return {'source_header': header, 'frames': frames}


def main():
    source = original_file('tutorialRep_001.plist')
    records = {key: decode(value) for key, value in plistlib.loads(source.read_bytes()).items()}
    target = ROOT / 'assets/configs/fly/tutorial.json'
    target.write_text(json.dumps(records, indent=2), encoding='utf-8')
    report = {key: {'count': len(value['frames']), 'controlled': sum(f is not None for f in value['frames']),
                    'source_header': value['source_header']} for key, value in records.items()}
    (ROOT/'docs/development/test-evidence/tutorial-import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
