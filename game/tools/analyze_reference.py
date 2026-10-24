"""Read-only reference inventory; reproducible evidence for the recreation."""
from pathlib import Path
import collections, csv, hashlib, json, re, xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / 'reference/Fly2theMoon_1.1_ios_3.2'
OUT = ROOT / 'docs/development'

def loose_json(path):
    text = path.read_text(encoding='utf-8-sig')
    return json.loads(re.sub(r',\s*([}\]])', r'\1', text))

def original_file(name):
    with (REF/'整理清单.csv').open(encoding='utf-8-sig',newline='') as f:
        for row in csv.DictReader(f):
            if row['original_path'].replace('\\','/').split('/')[-1] == name:
                return REF / row['new_path']
    raise FileNotFoundError(name)

def main():
    files = sorted(p for p in REF.rglob('*') if p.is_file())
    with (OUT / 'reference-inventory.csv').open('w', encoding='utf-8-sig', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['path','bytes','sha256'])
        for p in files:
            writer.writerow([p.relative_to(REF).as_posix(),p.stat().st_size,hashlib.sha256(p.read_bytes()).hexdigest()])
    calls, headers, layers, objects = collections.Counter(), collections.Counter(), collections.Counter(), collections.defaultdict(collections.Counter)
    levels=[]
    for folder in sorted((REF / '01_关卡').iterdir()):
        xml = ET.parse(folder / folder.name).getroot()
        script = original_file(folder.name+'.lua').read_text(encoding='utf-8-sig')
        calls.update(re.findall(r'\b([a-zA-Z]+\.[a-zA-Z]+)\s*\(',script))
        header = xml.find('Header')
        headers.update(e.tag for e in header)
        detail=[]
        for layer in xml.findall('Layer'):
            name = layer.get('Name')
            layers[name] += len(layer)
            objects[name].update(o.get('SeqID', o.tag) for o in layer)
            detail.append({'name': name, 'count':len(layer)})
        levels.append({'id':int(xml.get('ID')), 'width':xml.get('Width'),'height':xml.get('Height'),
                       'header':[{e.tag:e.attrib} for e in header], 'layers':detail,
                       'dialog_keys':sorted(set(re.findall(r'"(Level[^"\n]+)"',script))),
                       'callbacks':re.findall(r'(\w+)\s*=\s*function',script)})
    summary={'files':len(files),'bytes':sum(p.stat().st_size for p in files),
             'categories':dict(collections.Counter(p.relative_to(REF).parts[0] for p in files)),
             'extensions':dict(collections.Counter(p.suffix for p in files)),
             'script_calls':dict(calls),'header_types':dict(headers),'layer_objects':dict(layers),
             'object_ids':{k:dict(v) for k,v in objects.items()},'levels':levels}
    (OUT/'reference-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('levels','object_ids')},ensure_ascii=False,indent=2))

if __name__=='__main__': main()
