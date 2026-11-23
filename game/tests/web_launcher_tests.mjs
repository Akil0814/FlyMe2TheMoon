// Contract checks for the page launcher; not Android touch or browser rendering acceptance.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
class Target{
  handlers=new Map();hidden=false;disabled=false;value='';textContent='';
  style={values:new Map(),setProperty(key,value){this.values.set(key,value);}};
  addEventListener(type,callback){const handlers=this.handlers.get(type)||[];handlers.push(callback);this.handlers.set(type,handlers);}
  async fire(type,fields={}){const event={preventDefault(){this.prevented=true;},stopImmediatePropagation(){this.stopped=true;},...fields};
    for(const handler of this.handlers.get(type)||[]){await handler(event);if(event.stopped)break;}return event;}
  setAttribute(key,value){this[key]=value;}focus(){}click(){return this.fire('click');}
  getBoundingClientRect(){return {left:0,top:56,right:390,bottom:844};}
}
const tick=()=>new Promise(setImmediate);
async function launch({mode='landscape',storage=true,missing=false}={}){
  const ids=new Map(),get=id=>{if(!ids.has(id))ids.set(id,new Target());return ids.get(id);};
  const classes=new Set(),fetches=[],scripts=[],locations=[],syncs=[];let cancelled=0,pauses=0;
  const body=Object.assign(new Target(),{classList:{add:key=>classes.add(key),remove:key=>classes.delete(key),toggle:(key,on)=>on?classes.add(key):classes.delete(key)}});
  const document=Object.assign(new Target(),{body,fullscreenEnabled:true,hidden:false,getElementById:get,
    querySelector:selector=>get(selector),createElement:()=>new Target()});
  const window=new Target();
  body.appendChild=node=>{scripts.push(node.src);queueMicrotask(()=>{
    if(node.src.includes('/FlyMe2TheMoon.js')){
      window.Module.FS={syncfs(populate,done){assert.equal(populate,false);syncs.push(done);}};
      window.Module._fly_web_cancel_pointer=()=>cancelled++;
      window.Module._fly_web_request_pause=()=>pauses++;
      window.Module.onStorageStatus(storage);window.Module.onRuntimeInitialized();
    }node.onload();
  });};
  const location={href:`https://example.test/projects/FlyMe2TheMoon/web/?mode=${mode}`,search:`?mode=${mode}`,assign:url=>locations.push(url)};
  const scope={window,document,location,innerWidth:1200,innerHeight:800,URL,URLSearchParams,AbortController,console:{log(){},warn(){},error(){}},
    requestAnimationFrame:callback=>queueMicrotask(callback),ProjectPageCore:{init:()=>({getLang:()=> 'en',setLang:()=>Promise.resolve()})},
    fetch:async url=>{fetches.push(url);if(url.includes('manifest.json')){const portrait=url.includes('runtime-portrait');return {ok:true,json:async()=>({version:1,gameVersion:'1.0.1',buildId:'test',runtime:'FlyMe2TheMoon.js',downloadBytes:3,
      ...(portrait?{displayMode:'portrait',logicalWidth:640,logicalHeight:960}:{}),packages:[{script:'assets-00.js',data:'assets-00.data',bytes:2}],files:{'FlyMe2TheMoon.wasm':{bytes:1}}})};}
      const length=url.includes('.wasm')?1:2;let read=false;return {ok:!missing,status:404,body:{getReader:()=>({read:async()=>read?{done:true}:(read=true,{done:false,value:new Uint8Array(length)})})}};
    }};
  vm.createContext(scope);
  for(const name of ['platform.js','flyme2themoon_web.js'])vm.runInContext(fs.readFileSync(new URL('../platform/web/'+name,import.meta.url),'utf8'),scope);
  await tick();
  return {get,window,document,classes,fetches,scripts,locations,syncs,tick,cancelled:()=>cancelled,pauses:()=>pauses};
}
for(const mode of ['landscape','portrait']){
  const app=await launch({mode});assert.equal(app.fetches.length,1,'Selecting a version must fetch only its manifest');
  assert.equal(app.get('canvas').width,mode==='portrait'?640:1280);assert.equal(app.get('canvas').height,mode==='portrait'?960:720);
  await app.get('startGame').click();await app.tick();await app.get('startGame').click();
  const directory=mode==='portrait'?'runtime-portrait':'runtime';
  assert(app.fetches.every(url=>url.startsWith('./'+directory+'/')),'Only selected resources may download');
  assert.equal(app.scripts.filter(url=>url.includes('/FlyMe2TheMoon.js')).length,1,'Only one runtime may start');
  await app.get('switchGame').click();await app.tick();
  assert.equal(app.locations.length,0,'Navigation must wait for persistent sync');assert.equal(app.syncs.length,1);
  app.syncs.shift()(null);await app.tick();assert(app.locations[0].endsWith('mode='+(mode==='portrait'?'landscape':'portrait')));
}
{
  const app=await launch({mode:'portrait'});await app.get('startGame').click();await app.tick();
  await app.get('switchGame').click();await app.tick();app.syncs.shift()(new Error('blocked'));await app.tick();
  assert.equal(app.locations.length,0);assert.equal(app.get('switchGame').disabled,false,'Failed save must permit a retry');
  assert(app.get('downloadDetail').textContent.includes('Could not save'));
  const canvas=app.get('canvas');await canvas.fire('pointerdown',{pointerType:'touch',isPrimary:true,pointerId:1});
  assert((await canvas.fire('pointerdown',{pointerType:'touch',isPrimary:false,pointerId:2})).stopped);
  assert((await canvas.fire('pointermove',{pointerType:'touch',isPrimary:false,pointerId:2})).stopped);
  const before=app.cancelled();await canvas.fire('pointercancel',{pointerType:'touch',isPrimary:false,pointerId:2});assert.equal(app.cancelled(),before);
  await canvas.fire('pointermove',{pointerId:1,clientX:391,clientY:400});
  assert((await canvas.fire('pointermove',{pointerId:1,clientX:200,clientY:400})).stopped,'Cancelled finger cannot resume on re-entry');
  await canvas.fire('pointerup',{pointerId:1});const paused=app.pauses();app.document.hidden=true;await app.document.fire('visibilitychange');
  assert.equal(app.pauses(),paused+1);app.document.hidden=false;await app.document.fire('visibilitychange');assert.equal(app.pauses(),paused+1);
  await app.get('leaveGame').click();assert(!app.classes.has('portrait-focused'));await app.get('resumeGame').click();assert(app.classes.has('portrait-focused'));
}
{
  const app=await launch({mode:'portrait',storage:false});await app.get('startGame').click();await app.tick();
  assert.equal(app.get('mobileStorageWarning').hidden,false);await app.get('switchGame').click();await app.tick();assert.equal(app.locations.length,0,'Temporary progress must not be silently discarded');
}
{
  const app=await launch({missing:true});await app.get('startGame').click();await app.tick();
  assert.equal(app.get('reloadGame').hidden,false);assert.equal(app.scripts.length,0,'A missing package must not execute a partial runtime');
}
console.log('PASS: lazy version loading, legacy manifest, 640x960 portrait, single runtime, flush before switch, save retry, touch cancellation, background pause, temporary storage and package failure');
