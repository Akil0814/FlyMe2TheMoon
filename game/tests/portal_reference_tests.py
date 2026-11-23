"""Read-only portal import/reference audit plus the QA planner's real resolver."""
from pathlib import Path
import json
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'game/tools'))
from plan_test_route import _portal_destination


def main():
    rows, differences = [], []
    for level in range(1, 61):
        original = ET.parse(ROOT / f'reference/Fly2theMoon_1.1_ios_3.2/01_关卡/Level_{level:03}/Level_{level:03}').getroot()
        data = json.loads((ROOT / f'assets/configs/levels/{level:03}.json').read_text(encoding='utf8'))
        portals = [e for e in data['entities'] if not e['item'] and e['kind'] == 115]
        originals = {int(obj.get('ID')): obj for obj in original.findall(".//Layer[@Name='DynamicObject']/Obj[@SeqID='115']")}
        assert len(originals) == len(portals), f'Level {level}: portal inventory differs'
        links = []
        for portal in portals:
            obj = originals[portal['id']]
            info = obj.find('Info')
            expected_info = {} if info is None else info.attrib
            assert portal['info'] == expected_info, f'Level {level}, portal {portal["id"]}: imported Info differs'
            if 'TargetPortalID' not in expected_info:
                continue
            target = int(expected_info['TargetPortalID'])
            assert target in originals, f'Level {level}: target object {target} absent from original'
            exit_info = originals[target].find('Info')
            assert exit_info is None or 'TargetPortalID' not in exit_info.attrib, f'Level {level}: target {target} is an entrance'
            destination = _portal_destination(portals, target)
            assert destination['id'] == target, f'Level {level}, entrance {portal["id"]}: planner selected {destination["id"]}, expected {target}'
            # Reproduce the removed rule only to quantify the fix's impact.
            old = next((p for p in reversed(portals) if int(p['info'].get('PortalID', -1)) == target), destination)
            link = {'source': portal['id'], 'original_target': target, 'target': destination['id']}
            links.append(link)
            if old['id'] != target:
                differences.append({'level': level, 'source': portal['id'], 'before': old['id'], 'after': target})
        rows.append({'level': level, 'portals': len(portals), 'links': links})
    assert differences == [{'level': 48, 'source': 14, 'before': 22, 'after': 5}], differences
    expected48 = {1: 16, 3: 16, 4: 16, 10: 22, 11: 19, 13: 2, 14: 5, 20: 16, 21: 16}
    assert {e['source']: e['target'] for e in rows[47]['links']} == expected48
    # A PortalID attribute alone must never manufacture an object-ID match.
    try:
        _portal_destination([{'id': 22, 'info': {'PortalID': '5'}}], 5)
    except ValueError:
        pass
    else:
        raise AssertionError('Planner fell back to Info.PortalID for a missing object')
    output = ROOT / 'out/qa/portal-reference-regression.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({'passed': True, 'changed_links': differences, 'levels': rows}, indent=2), encoding='utf8')
    print(f'PASS: all 60 original XML portal imports and planner links; only level 48 entrance 14 changes (22 -> 5); {output}')


if __name__ == '__main__':
    main()
