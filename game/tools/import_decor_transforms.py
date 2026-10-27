"""Refresh only decoration transform metadata; leave gameplay data untouched."""
from asset_usage import assets_root
ASSETS=assets_root()
from pathlib import Path
import json
from analyze_reference import ROOT, ET, original_file


def main():
    counts={'levels':0,'decor':0,'flip_x':0,'flip_y':0,'changed_files':0}
    for index in range(1,61):
        path=ASSETS / f'configs/levels/{index:03}.json'
        level=json.loads(path.read_text(encoding='utf8'))
        original=ET.parse(original_file(f'Level_{index:03}')).getroot()
        expected=[]
        for layer in original.findall('Layer'):
            name=layer.get('Name')
            if name not in ['Texture','Foreground','Farground','Background']:continue
            depths=[int(obj.get('Depth',0)) for obj in layer]
            # Supplied XML already follows native addChild:z order. Fail before
            # altering an existing list if a future reference needs reordering.
            assert depths==sorted(depths),(index,name,'depth order')
            for obj in layer:
                seq=int(obj.get('SeqID'))
                if name=='Background' and seq==1:continue
                prefix=f'far{level["suite"]}.{name}' if name in ['Farground','Background'] else f'terrain{level["suite"]}.{name}'
                expected.append((f'fly.{prefix}_{seq:03}',obj.attrib,name))
        assert len(expected)==len(level['decor']),(index,'unresolved decoration')
        changed=False
        for decor,(key,source,name) in zip(level['decor'],expected):
            assert decor['key']==key and decor['layer']==name,(index,key,'binding')
            assert decor['x']==float(source['X']) and decor['y']==level['height']-float(source['Y'])-decor['h'],(index,key,'placement')
            assert decor['rotation']==float(source.get('Rotation',0)) and decor['flip']==(source.get('IsXFlipped')=='true'),(index,key,'orientation')
            before=dict(decor)
            if source.get('IsYFlipped')=='true':decor['flip_y']=True
            else:decor.pop('flip_y',None)
            if 'Alpha' in source:decor['alpha']=max(0,min(255,int(source['Alpha'])))
            changed|=before!=decor
            counts['decor']+=1;counts['flip_x']+=decor['flip'];counts['flip_y']+=decor.get('flip_y',False)
        if changed:
            path.write_text(json.dumps(level,ensure_ascii=False,indent=2),encoding='utf8');counts['changed_files']+=1
        counts['levels']+=1
    output=ROOT/'out/qa/decor-transform-import.json';output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(counts,indent=2),encoding='utf8')
    print(json.dumps(counts))


if __name__=='__main__':
    main()
