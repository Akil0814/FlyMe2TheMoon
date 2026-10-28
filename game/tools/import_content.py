"""Convert supplied reference into game data, never modifying source files."""
from asset_usage import assets_root, prune_imported, digest, read
from pathlib import Path
import json, math, shutil, struct, re, collections
from PIL import Image
from analyze_reference import ROOT, REF, OUT, loose_json, original_file, ET
from import_tutorial import main as import_tutorial
from import_tutorial_visual import main as import_tutorial_visual

ASSETS=assets_root()
TEXTURES=ASSETS/'textures'
CONFIG=ASSETS/'configs'
SPRITES=REF/'08_PNG解析/03_拆分帧'

def write_json(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def main():
    import_tutorial()
    textures={}; sizes={}; animations=[]
    custom_hd={entry['path']:entry for entry in read(ROOT/'game/tools/custom_hd_assets.json')}
    def texture(key,p,scale=0.5):
        target=TEXTURES/(key.replace('.','/')+'.png');target.parent.mkdir(parents=True,exist_ok=True)
        custom=custom_hd.get(target.relative_to(ASSETS).as_posix())
        if not (custom and target.is_file() and digest(target)==custom['sha256']):
            shutil.copyfile(p,target)
        textures['fly.'+key]={'path':target.relative_to(ASSETS/'textures').as_posix()}
        with Image.open(target) as image:w,h=image.size
        sizes[key]=[w*scale,h*scale]
        return key
    for suite in range(1,5):
        for group,prefix in [(f'TextureS{suite}',f'terrain{suite}'),(f'FargroundsS{suite}',f'far{suite}')]:
            for p in (SPRITES/group/(group+'-hd')).glob('*.png'):
                texture(prefix+'.'+p.stem,p)
    for group in ['DynamicObjectsLite','InGameUI','TutorialUI','LevelSelect','Panel','Options']:
        for p in (SPRITES/group/(group+'-hd')).glob('*.png'):
            texture(group+'.'+p.stem,p)
    for group in ['CharActions','DynamicObjectsLite']:
        folder=SPRITES/group/(group+'-hd')
        for path in sorted(p for p in folder.rglob('*') if p.is_dir()):
            frames=sorted(path.glob('*.png'))
            if not frames:continue
            key=group+'.'+'.'.join(path.relative_to(folder).parts)
            target=TEXTURES/'animation'/key;target.mkdir(parents=True,exist_ok=True)
            for index,p in enumerate(frames):shutil.copyfile(p,target/f'frame_{index:03}.png')
            # RoleView frame-rate table at 0x1ac9a8; mechanism rates are separate.
            role_fps={'charStand':25,'charFree':25,'charUp':25,'charLeft':25,'charRight':25,'charDown':25,
                      'charIdle':10,'charLookLeft':10,'charLookRight':10,'charLookLeftRight':10,'charDeath':25}
            fps=role_fps.get(path.name,12) if group=='CharActions' else 12
            animations.append({'key':'fly.'+key,'path':target.relative_to(ASSETS/'textures').as_posix(),
                'frame_prefix':'frame','frame_count':len(frames),'fps':fps,'loop':True})
            w,h=Image.open(frames[0]).size;sizes[key]=[w/2,h/2]
            texture(key+'.first',frames[0])
    single=REF/'08_PNG解析/01_独立图片'
    for p in single.rglob('*.png'):
        # The credits page now uses a complete widescreen background.
        if p.stem in {'creditsTop-hd','creditsBottom-hd'}:continue
        if p.name=='stars.png':texture('laser_stars',p,1)
        if p.stem.endswith('-hd') and any(s in str(p) for s in ['剧情与背景','角色表情']):
            texture('ui.'+p.stem.removesuffix('-hd'),p)
    # Background images have different source groups; retain unique decoded paths.
    for p in single.rglob('*.png'):
        if re.match(r'Background',p.name,re.I):texture('background.'+p.stem.replace('-','_'),p)
    meta=loose_json(original_file('edgeDataMeta.json'));raw=original_file('edgeData.dat').read_bytes()
    edges={}
    for key,v in meta.items():
        assert v['offset']+v['count']*8<=len(raw)
        pairs=struct.unpack_from('<'+str(v['count']*2)+'f',raw,v['offset'])
        edges[key]=list(zip(pairs[::2],pairs[1::2]))
    resources=ET.parse(original_file('resourcesLite.xml')).getroot()
    refs={e.get('SeqID'):[dict(x.attrib) for x in e.findall('EdgeRef')] for e in resources.findall(".//Res[@TypeID='2']")}
    levels=[]; unresolved=[]
    for index in range(1,61):
        root=ET.parse(original_file(f'Level_{index:03}')).getroot();header=root.find('Header')
        w,h=float(root.get('Width')),float(root.get('Height'));suite=header.find('Suite')
        suite=int(suite.get('ID')) if suite is not None else 1
        born=header.find('BornPoint')
        level={'id':index,'width':w,'height':h,'suite':suite,'born':[float(born.get('X')),h-float(born.get('Y'))],
               'decor':[],'edges':[],'entities':[],'areas':[], 'script':original_file(f'Level_{index:03}.lua').read_text(encoding='utf-8-sig')}
        for layer in root.findall('Layer'):
            name=layer.get('Name')
            objects=sorted(layer,key=lambda obj:int(obj.get('Depth',0))) if name in ['Texture','Foreground','Farground','Background'] else layer
            for obj in objects:
                a=obj.attrib;x=float(a['X']);y=float(a['Y']);rotation=float(a.get('Rotation',0));seq=int(a.get('SeqID',0))
                if name=='StaticObject':
                    for ref in refs[str(seq)]:
                        points=[]
                        for px,py in edges[ref['EdgeID']]:
                            rad=-math.radians(float(ref.get('Rotation',0)));scale=float(ref.get('Scale',1))
                            px,py=(px*math.cos(rad)-py*math.sin(rad))*scale+float(ref.get('X',0)),(px*math.sin(rad)+py*math.cos(rad))*scale+float(ref.get('Y',0))
                            # StaticObjectLayer.load: mirrors the assembled edge
                            # before object rotation (Edge 0x97e34..0x97ea4).
                            if a.get('IsXFlipped')=='true':px=-px
                            if a.get('IsYFlipped')=='true':py=-py
                            rad=-math.radians(rotation);scale=float(a.get('Scale',1))
                            px,py=(px*math.cos(rad)-py*math.sin(rad))*scale,(px*math.sin(rad)+py*math.cos(rad))*scale
                            points.append([x+px,h-y-py])
                        level['edges'].append(points)
                elif name in ['Texture','Foreground','Farground','Background']:
                    # The original BackgroundLayer.load: explicitly skips SeqID 1
                    # (0xa7644..0xa764c); the base sky is a procedural gradient.
                    if name=='Background' and seq==1:continue
                    prefix=f'far{suite}.{name}' if name in ['Farground','Background'] else f'terrain{suite}.{name}'
                    key=f'{prefix}_{seq:03}'
                    if key not in sizes:
                        unresolved.append([index,name,seq,key]);continue
                    sw,sh=sizes[key]
                    decor={'key':'fly.'+key,'x':x,'y':h-y-sh,'w':sw,'h':sh,'rotation':rotation,
                           'flip':a.get('IsXFlipped')=='true','layer':name,'layerWidth':float(layer.get('Width')),'layerHeight':float(layer.get('Height'))}
                    if a.get('IsYFlipped')=='true':decor['flip_y']=True
                    if 'Alpha' in a:decor['alpha']=max(0,min(255,int(a['Alpha'])))
                    level['decor'].append(decor)
                elif name in ['DynamicObject','Item']:
                    key=f'DynamicObjectsLite.{"Item" if name=="Item" else "DynamicObject"}_{seq:03}'
                    sw,sh=sizes[key]
                    info=obj.find('Info')
                    # Cocos editor placements use the lower-left image anchor;
                    # rotate its local half-size before converting to body center.
                    rad=math.radians(rotation) if seq not in [8,24,25] else 0
                    cx=x+sw/2*math.cos(rad)+sh/2*math.sin(rad)
                    cy=h-y+sw/2*math.sin(rad)-sh/2*math.cos(rad)
                    level['entities'].append({'kind':seq,'item':name=='Item','id':int(a['ID']), 'x':cx,'y':cy,
                        'w':sw,'h':sh,'rotation':rotation,'key':'fly.'+key,'info':dict(info.attrib) if info is not None else {}})
                elif name=='Area':
                    level['areas'].append({'x':x,'y':h-y-float(a['Height']),'w':float(a['Width']),'h':float(a['Height']),
                        'actions':[dict(action.attrib) for action in obj.findall('.//Action')]})
        levels.append(level)
        write_json(CONFIG/f'levels/{index:03}.json',level)
    write_json(CONFIG/'catalog.json',{'sizes':{'fly.'+k:v for k,v in sizes.items()},'dependencies':loose_json(original_file('LevelDependency.json')),
        'selection':loose_json(original_file('LevelSelectData.json')),'score':loose_json(original_file('levelScore.json')),
        'role':loose_json(original_file('roleConfig.json'))})
    for lang in ['zh-Hans','en','ja']:
        folder=REF/'04_本地化'/lang/'文本'
        if not folder.exists():continue
        for p in folder.glob('*.json'):
            txt=p.read_text(encoding='utf-8-sig').replace("\\'","'")
            txt=re.sub(r',\s*([}\]])',r'\1',txt)
            write_json(CONFIG/'text'/lang/p.name,json.loads(txt))
    manifest=ASSETS/'configs/manifests'
    old=loose_json(manifest/'textures_manifest.json');old['textures'].update(textures)
    write_json(manifest/'textures_manifest.json',old)
    old=loose_json(manifest/'animations_manifest.json');old['animations']=list({x['key']:x for x in old['animations']+animations}.values())
    write_json(manifest/'animations_manifest.json',old)
    import_tutorial_visual()
    prune_imported(ASSETS)
    report={'levels':len(levels),'textures':len(textures),'animations':len(animations),'unresolved_decor':unresolved}
    write_json(OUT/'test-evidence/content-import.json',report)
    print(json.dumps(report,ensure_ascii=False)[:2500])

if __name__=='__main__':main()
