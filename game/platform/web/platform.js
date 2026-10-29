(() => {
  'use strict';
  const canvas=document.getElementById('canvas'),host=document.getElementById('gameViewport'),resume=document.getElementById('resumeGame');
  let portrait=false,running=false,pointer=null,cancelled=null;
  const ignored=new Set();
  const block=event=>{event.preventDefault();event.stopImmediatePropagation();};
  const secondary=event=>{
    if(ignored.has(event.pointerId)||(event.pointerType==='touch'&&event.isPrimary===false)){block(event);return true;}
    return false;
  };
  const cancel=()=>{if(pointer!==null)cancelled=pointer;pointer=null;window.Module?._fly_web_cancel_pointer?.();};
  const pause=()=>{cancel();window.Module?._fly_web_request_pause?.();};
  function enter(){if(!portrait)return;document.body.classList.add('portrait-focused');if(running)canvas.focus({preventScroll:true});}
  function leave(){pause();if(document.fullscreenElement===host)void document.exitFullscreen();document.body.classList.remove('portrait-focused');resume.hidden=!running||!portrait;resume.focus({preventScroll:true});}
  document.getElementById('leaveGame').addEventListener('click',leave);
  document.getElementById('mobileFullscreen').addEventListener('click',()=>document.getElementById('fullscreenGame').click());resume.addEventListener('click',enter);
  canvas.addEventListener('pointerdown',event=>{
    if(event.pointerType!=='touch')return;if(secondary(event))return;
    if(pointer!==null&&pointer!==event.pointerId){ignored.add(event.pointerId);block(event);return;}
    cancelled=null;pointer=event.pointerId;
  },true);
  canvas.addEventListener('pointermove',event=>{
    if(secondary(event))return;if(event.pointerId===cancelled){block(event);return;}if(pointer!==event.pointerId)return;
    const r=canvas.getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom){cancel();block(event);}
  },true);
  canvas.addEventListener('pointerup',event=>{
    if(secondary(event)){ignored.delete(event.pointerId);return;}
    if(event.pointerId===cancelled){cancelled=null;window.Module?._fly_web_cancel_pointer?.();}if(event.pointerId===pointer)pointer=null;
  },true);
  canvas.addEventListener('pointercancel',event=>{
    if(secondary(event)){ignored.delete(event.pointerId);return;}if(event.pointerId===pointer)cancel();if(event.pointerId===cancelled)cancelled=null;
  },true);
  canvas.addEventListener('pointerleave',event=>{if(event.pointerId===pointer)cancel();});
  canvas.addEventListener('lostpointercapture',event=>{if(event.pointerId===pointer)cancel();});
  window.addEventListener('blur',()=>{if(running)pause();});document.addEventListener('visibilitychange',()=>{if(running&&document.hidden)pause();});
  window.addEventListener('orientationchange',()=>{if(running)pause();});
  window.FlyWebPlatform={configure(mode){portrait=mode==='portrait';document.body.classList.toggle('portrait-game',portrait);document.getElementById('mobileToolbar').hidden=!portrait;},
    start:enter,ready(){running=true;resume.hidden=!portrait;},pause,stopped(){leave();running=false;resume.hidden=true;}};
})();
