"""Normalize the supplied original audio with installed FFmpeg; retain sources."""
from asset_usage import assets_root, prune_imported
ASSETS=assets_root()
from analyze_reference import ROOT,REF,OUT
import shutil,subprocess,json,struct,wave
manifest=ASSETS / 'configs/manifests/audio_manifest.json'
data=json.loads(manifest.read_text(encoding='utf-8'))
for group in ['sounds','music']:
    data[group]={k:v for k,v in data[group].items() if not k.startswith('fly.')}
ffmpeg=shutil.which('ffmpeg')
assert ffmpeg,'FFmpeg is required to import CAF and ADPCM audio'
report=[]
for path in sorted((REF/'03_音频').rglob('*')):
    if path.suffix.lower() not in ['.wav','.caf']:continue
    group='music' if path.suffix.lower()=='.caf' else 'sounds'
    target=ASSETS / 'audio'/group/(path.stem+'.wav')
    target.parent.mkdir(parents=True,exist_ok=True)
    source=path;repair=None
    if path.suffix.lower()=='.caf':
        raw=bytearray(path.read_bytes());assert raw[:4]==b'caff'
        chunks={};pos=8
        while pos+12<=len(raw):
            kind,length=struct.unpack_from('>4sq',raw,pos);assert length>=0 and pos+12+length<=len(raw)
            chunks[kind]=(pos+12,length);pos+=12+length
        desc=chunks[b'desc'][0];packet_bytes=struct.unpack_from('>I',raw,desc+16)[0]
        packet_frames=struct.unpack_from('>I',raw,desc+20)[0]
        table=chunks[b'pakt'][0];count,valid,priming,remainder=struct.unpack_from('>qqii',raw,table)
        size=chunks[b'data'][1]-4;assert size%packet_bytes==0
        if count==0:
            count=size//packet_bytes;assert count*packet_frames==valid+priming+remainder
            struct.pack_into('>q',raw,table,count)
            source=ROOT/'out/import-audio'/path.name;source.parent.mkdir(parents=True,exist_ok=True);source.write_bytes(raw)
            repair={'zero_packet_count_corrected':count,'valid_frames':valid,'priming':priming,'remainder':remainder}
    conversion=subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-y','-i',str(source),'-c:a','pcm_s16le',str(target)],check=True,capture_output=True,text=True,encoding='utf-8',errors='replace')
    with wave.open(str(target),'rb') as pcm:
        frames=pcm.getnframes();rate=pcm.getframerate();channels=pcm.getnchannels()
        assert frames>0 and pcm.getsampwidth()==2 and pcm.getcomptype()=='NONE'
        if repair:assert frames==repair['valid_frames'],f'{path.name}: decoded audio incomplete'
        else:
            with wave.open(str(path),'rb') as original:
                assert frames==original.getnframes() and rate==original.getframerate() and channels==original.getnchannels()
    key='fly.'+('voice_'+path.stem.replace('-','_') if group=='sounds' else path.stem)
    data[group][key]={'path':target.relative_to(ASSETS / 'audio').as_posix()}
    report.append({'source':str(path.relative_to(REF)),'output':'assets/'+target.relative_to(ASSETS).as_posix(),'bytes':target.stat().st_size,'container_repair':repair,'frames':frames,'rate':rate,'channels':channels,'diagnostics':conversion.stderr})
# 44-1 is an original filename archived in 99_重复文件; its content equals 42-1.
data['sounds']['fly.voice_44_1']={'path':'sounds/42-1.wav'}
manifest.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
(OUT/'test-evidence/audio-import.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
prune_imported(ASSETS)
print(f'Imported {len(report)} original audio files')
