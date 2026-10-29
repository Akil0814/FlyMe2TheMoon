(() => {
  'use strict';
  const element=id=>document.getElementById(id);
  const canvas=element('canvas'),host=element('gameViewport'),selector=element('displayMode');
  const start=element('startGame'),reload=element('reloadGame'),switcher=element('switchGame');
  const fullscreen=element('fullscreenGame'),exitFullscreen=element('exitFullscreen');
  const modes={landscape:{directory:'runtime',width:1280,height:720},portrait:{directory:'runtime-portrait',width:640,height:960}};
  const requested=new URLSearchParams(location.search).get('mode');
  selector.value=Object.hasOwn(modes,requested)?requested:(innerHeight>innerWidth?'portrait':'landscape');
  const state={phase:'idle',downloaded:0,total:0,storage:null,error:'',saving:false};
  const buffers=new Map(),abort=new AbortController();
  let manifest=null,manifestRequest=0,fullscreenPending=false;
  const t=(key,fallback)=>window.I18N?.t(`flyme2themoon_web.${key}`,undefined,fallback)??fallback;
  const mib=bytes=>(bytes/1048576).toFixed(1);
  function syncFullscreen(){
    const active=document.fullscreenElement===host;
    exitFullscreen.hidden=!active;exitFullscreen.disabled=fullscreenPending;
    fullscreen.disabled=fullscreenPending||state.phase!=='running'||!document.fullscreenEnabled;
    fullscreen.setAttribute('aria-pressed',String(active));
    fullscreen.textContent=t(active?'actions.exit_fullscreen':'actions.fullscreen',active?'Exit Fullscreen':'Fullscreen');
    const mobile=element('mobileFullscreen');mobile.disabled=fullscreen.disabled;mobile.textContent=fullscreen.textContent;
  }
  function render(){
    syncFullscreen();
    element('status').textContent=t(`runtime.${state.saving?'saving':state.phase}`,state.phase);
    const progress=element('downloadProgress');progress.hidden=state.phase!=='loading';progress.max=state.total||1;progress.value=state.downloaded;
    const detail=element('downloadDetail');
    detail.textContent=state.error||(state.phase==='idle'
      ?`${t('runtime.size','Full game · first launch downloads resources')}${manifest?` · ${mib(manifest.downloadBytes)} MiB`:''}`
      :`${mib(state.downloaded)} / ${mib(state.total)} MiB`);
    const storage=element('storageStatus');storage.hidden=state.storage!==false;
    storage.textContent=t('runtime.temporary','Browser storage is unavailable. Progress will be lost when this page closes.');
    const warning=element('mobileStorageWarning');warning.hidden=state.storage!==false;
    warning.textContent=t('runtime.temporary_short','Saves off');warning.title=storage.textContent;
    selector.disabled=state.phase!=='idle'||state.saving;reload.disabled=state.saving;switcher.disabled=state.saving;
    if(state.phase==='loading')document.querySelector('#gameCover p').textContent=`${t('runtime.loading','Downloading / initializing')} · ${detail.textContent}`;
    if(state.phase==='error')document.querySelector('#gameCover p').textContent=state.error;
    document.querySelector('[data-i18n="flyme2themoon_web.runtime.controls"]').textContent=
      selector.value==='portrait'?t('runtime.touch_controls','Tap menus. Hold below Kiana and drag to fly; release to stop.'):t('runtime.controls','Click the canvas to focus. Space: thrust; P: pause; Esc: back.');
  }
  function fail(error){
    if(state.phase==='error')return;
    console.error('FlyMe2TheMoon:',error);state.phase='error';state.error=t('runtime.failed','The game could not start. Reload the page to try again.');
    window.FlyWebPlatform.stopped();reload.hidden=false;start.disabled=true;buffers.clear();abort.abort();render();
  }
  async function selectManifest(){
    start.disabled=true;manifest=null;state.error='';const request=++manifestRequest,mode=selector.value;
    try{
      const response=await fetch(`./${modes[mode].directory}/manifest.json`,{cache:'no-store'});
      if(!response.ok)throw new Error('Web manifest is unavailable');
      const value=await response.json();if(request!==manifestRequest)return;
      if(value.version!==1||!Array.isArray(value.packages)||!value.files)throw new Error('Unsupported manifest');
      const width=value.logicalWidth??modes[mode].width,height=value.logicalHeight??modes[mode].height;
      if(width!==modes[mode].width||height!==modes[mode].height)throw new Error('Build presentation does not match selection');
      if(mode==='portrait'&&value.displayMode!=='portrait')throw new Error('Expected portrait build');
      manifest=value;canvas.width=width;canvas.height=height;
      host.style.setProperty('--game-ratio',String(width/height));host.style.setProperty('--game-inverse',String(height/width));
      window.FlyWebPlatform.configure(mode);element('gameVersion').textContent=`v${value.gameVersion||'1.0.0'}`;
      element('gameNotices').href=`./${modes[mode].directory}/THIRD_PARTY_NOTICES.md`;
      render();start.disabled=false;
    }catch(error){if(request===manifestRequest)fail(error);}
  }
  async function download(url,expected){
    const response=await fetch(url,{signal:abort.signal});if(!response.ok)throw new Error(`${response.status}: ${url}`);
    const result=new Uint8Array(expected),reader=response.body.getReader();let offset=0;
    for(;;){const {done,value}=await reader.read();if(done)break;
      if(offset+value.length>expected)throw new Error('Unexpected resource size');result.set(value,offset);offset+=value.length;state.downloaded+=value.length;render();}
    if(offset!==expected)throw new Error('Incomplete resource');return result.buffer;
  }
  function script(url){return new Promise((resolve,reject)=>{
    const node=document.createElement('script');node.src=url;node.onload=resolve;node.onerror=()=>reject(new Error('Could not load '+url));document.body.appendChild(node);
  });}
  function resumeAudio(){const audio=window.Module?.SDL3?.audioContext;if(audio?.state==='suspended')void audio.resume().catch(console.warn);}
  function focusGame(){canvas.focus({preventScroll:true});resumeAudio();}
  canvas.addEventListener('pointerdown',focusGame);
  canvas.addEventListener('keydown',event=>{if(['Space','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(event.code))event.preventDefault();resumeAudio();});
  canvas.addEventListener('contextmenu',event=>event.preventDefault());
  canvas.addEventListener('webglcontextlost',event=>{event.preventDefault();fail('WebGL context lost');});
  async function navigate(mode){
    if(state.saving)return;state.saving=true;state.error='';render();
    try{
      window.FlyWebPlatform.pause();
      if(window.Module?.FS){
        if(state.storage!==true)throw new Error('Persistent storage unavailable');
        await new Promise(resolve=>requestAnimationFrame(resolve));
        await new Promise((resolve,reject)=>window.Module.FS.syncfs(false,error=>error?reject(error):resolve()));
      }
      const url=new URL(location.href);url.searchParams.set('mode',mode);location.assign(url.href);
    }catch(error){console.warn('Save before navigation failed:',error);state.saving=false;state.error=t('runtime.save_failed','Could not save. The game stays open; retry after storage is available.');render();}
  }
  reload.addEventListener('click',()=>void navigate(selector.value));
  switcher.addEventListener('click',()=>void navigate(selector.value==='portrait'?'landscape':'portrait'));
  async function toggleFullscreen(){
    if(fullscreenPending)return;fullscreenPending=true;syncFullscreen();
    try{if(document.fullscreenElement===host)await document.exitFullscreen();else await host.requestFullscreen();focusGame();}
    catch(error){console.warn('Fullscreen unavailable:',error);}finally{fullscreenPending=false;syncFullscreen();}
  }
  fullscreen.addEventListener('click',()=>void toggleFullscreen());exitFullscreen.addEventListener('click',()=>void toggleFullscreen());
  document.addEventListener('fullscreenchange',()=>{syncFullscreen();if(state.phase==='running')focusGame();});
  document.addEventListener('keydown',event=>{
    if(event.key==='Escape'&&document.fullscreenElement===host){event.preventDefault();event.stopImmediatePropagation();void toggleFullscreen();}
  },true);
  selector.addEventListener('change',()=>{if(state.phase==='idle')void selectManifest();});
  start.addEventListener('click',async()=>{
    if(state.phase!=='idle'||!manifest)return;
    start.disabled=true;state.phase='loading';window.FlyWebPlatform.start();render();
    const selected=manifest,directory=modes[selector.value].directory;
    const locate=path=>`./${directory}/${path}?v=${encodeURIComponent(selected.buildId)}`;
    try{
      const wasm=selected.runtime.replace(/\.js$/,'.wasm');state.total=selected.packages.reduce((sum,p)=>sum+p.bytes,selected.files[wasm].bytes);
      await Promise.all(selected.packages.map(async p=>buffers.set(p.data,await download(locate(p.data),p.bytes))));
      const wasmBinary=await download(locate(wasm),selected.files[wasm].bytes);
      window.Module={canvas,wasmBinary,locateFile:locate,thisProgram:'/',
        getPreloadedPackage(url){const name=new URL(url,location.href).pathname.split('/').pop(),buffer=buffers.get(name);buffers.delete(name);return buffer;},
        onStorageStatus(available){state.storage=available;render();},
        onRuntimeInitialized(){state.phase='running';canvas.hidden=false;element('gameCover').hidden=true;reload.hidden=false;switcher.hidden=false;window.FlyWebPlatform.ready();render();focusGame();},
        onGameExit(code){window.FlyWebPlatform.stopped();if(code){fail('Game exited with an error');return;}state.phase='exited';render();},
        onAbort:fail,print:(...args)=>console.log(...args),printErr:(...args)=>console.error(...args)};
      for(const p of selected.packages)await script(locate(p.script));await script(locate(selected.runtime));
    }catch(error){fail(error);}
  });
  window.addEventListener('error',event=>{if(['loading','running'].includes(state.phase))fail(event.message);});
  window.addEventListener('unhandledrejection',event=>{if(['loading','running'].includes(state.phase))fail(event.reason);});
  const page=ProjectPageCore.init({onLangChange:render});page.setLang(page.getLang()).then(selectManifest).catch(fail);
})();
