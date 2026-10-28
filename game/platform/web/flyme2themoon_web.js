(() => {
  'use strict';
  const canvas = document.getElementById('canvas');
  const viewport = document.getElementById('gameViewport');
  const exitFullscreen = document.getElementById('exitFullscreen');
  const status = document.getElementById('status');
  const progress = document.getElementById('downloadProgress');
  const detail = document.getElementById('downloadDetail');
  const storage = document.getElementById('storageStatus');
  const start = document.getElementById('startGame');
  const fullscreen = document.getElementById('fullscreenGame');
  const reload = document.getElementById('reloadGame');
  const state = {mode: 'idle', downloaded: 0, total: 0, storage: null, error: ''};
  const buffers = new Map();
  const downloadAbort = new AbortController();
  let fullscreenPending = false;
  const t = (key, fallback) => window.I18N?.t(`flyme2themoon_web.${key}`, undefined, fallback) ?? fallback;
  const mib = bytes => (bytes / 1048576).toFixed(1);
  function render() {
    syncFullscreenControls();
    status.textContent = t(`runtime.${state.mode}`, state.mode);
    progress.hidden = state.mode !== 'loading';
    progress.max = state.total || 1;
    progress.value = state.downloaded;
    detail.textContent = state.error || (state.total
      ? `${mib(state.downloaded)} / ${mib(state.total)} MiB`
      : t('runtime.size', 'Full game · approximately 145 MiB of assets'));
    storage.hidden = state.storage !== false;
    storage.textContent = t('runtime.temporary', 'Browser storage is unavailable. Progress will be lost when this page closes.');
  }
  function fail(error) {
    if (state.mode === 'error') return;
    console.error('FlyMe2TheMoon:', error);
    state.mode = 'error';
    state.error = t('runtime.failed', 'The game could not start. Reload the page to try again.');
    reload.hidden = false;
    buffers.clear();
    downloadAbort.abort();
    render();
  }
  async function download(url, expected) {
    const response = await fetch(url, {signal: downloadAbort.signal});
    if (!response.ok) throw new Error(`${response.status}: ${url}`);
    const result = new Uint8Array(expected);
    const reader = response.body.getReader();
    let offset = 0;
    for (;;) {
      const {done, value} = await reader.read();
      if (done) break;
      if (offset + value.length > expected) throw new Error('Unexpected resource size: ' + url);
      result.set(value, offset);
      offset += value.length;
      state.downloaded += value.length;
      render();
    }
    if (offset !== expected) throw new Error('Incomplete resource: ' + url);
    return result.buffer;
  }
  function script(url) {
    return new Promise((resolve, reject) => {
      const element = document.createElement('script');
      element.src = url;
      element.onload = resolve;
      element.onerror = () => reject(new Error('Could not load ' + url));
      document.body.appendChild(element);
    });
  }
  function resumeAudio() {
    const audio = window.Module?.SDL3?.audioContext;
    if (audio?.state === 'suspended') audio.resume().catch(console.warn);
  }
  const focusGame = () => { canvas.focus({preventScroll:true}); resumeAudio(); };
  canvas.addEventListener('pointerdown', focusGame);
  canvas.addEventListener('keydown', event => {
    if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(event.code)) event.preventDefault();
    resumeAudio();
  });
  canvas.addEventListener('contextmenu', event => event.preventDefault());
  canvas.addEventListener('webglcontextlost', event => { event.preventDefault(); fail('WebGL context lost'); });
  reload.addEventListener('click', () => location.reload());
  function syncFullscreenControls() {
    const active = document.fullscreenElement === viewport;
    exitFullscreen.hidden = !active;
    exitFullscreen.disabled = fullscreenPending;
    fullscreen.disabled = fullscreenPending || state.mode !== 'running';
    fullscreen.setAttribute('aria-pressed', String(active));
    fullscreen.textContent = t(active ? 'actions.exit_fullscreen' : 'actions.fullscreen', active ? 'Exit Fullscreen' : 'Fullscreen');
  }
  async function toggleFullscreen() {
    if (fullscreenPending) return;
    fullscreenPending = true;
    syncFullscreenControls();
    try {
      if (document.fullscreenElement === viewport) await document.exitFullscreen();
      // SDL observes fullscreen events on its canvas and changes its viewport.
      // Fullscreen the host instead, and let CSS scale the complete 16:9 image.
      else await viewport.requestFullscreen();
      focusGame();
    } catch (error) { console.warn('Fullscreen unavailable:', error); }
    finally { fullscreenPending = false; syncFullscreenControls(); }
  }
  fullscreen.addEventListener('click', toggleFullscreen);
  exitFullscreen.addEventListener('click', toggleFullscreen);
  document.addEventListener('fullscreenchange', () => {
    syncFullscreenControls();
    if (state.mode === 'running') focusGame();
  });
  // Exiting fullscreen must not also trigger the game's Escape/back action.
  document.addEventListener('keydown', event => {
    if (event.key !== 'Escape' || document.fullscreenElement !== viewport) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    if (!fullscreenPending) void toggleFullscreen();
  }, true);
  start.addEventListener('click', async () => {
    start.disabled = true;
    state.mode = 'loading';
    render();
    try {
      const response = await fetch('./runtime/manifest.json', {cache: 'no-store'});
      if (!response.ok) throw new Error('Web build manifest is missing');
      const manifest = await response.json();
      document.getElementById('gameVersion').textContent = `v${manifest.gameVersion || '1.0.0'}`;
      const locate = path => `./runtime/${path}?v=${encodeURIComponent(manifest.buildId)}`;
      const wasm = manifest.runtime.replace(/\.js$/, '.wasm');
      state.total = manifest.packages.reduce((sum, p) => sum + p.bytes, manifest.files[wasm].bytes);
      // Download before executing generated loaders, so failures never leave a silent dependency wait.
      await Promise.all(manifest.packages.map(async p => buffers.set(p.data, await download(locate(p.data), p.bytes))));
      const wasmBinary = await download(locate(wasm), manifest.files[wasm].bytes);
      window.Module = {
        canvas, wasmBinary, locateFile: locate, thisProgram: '/',
        getPreloadedPackage(url) {
          const name = new URL(url, location.href).pathname.split('/').pop();
          const buffer = buffers.get(name);
          buffers.delete(name);
          return buffer;
        },
        onStorageStatus(available) { state.storage = available; render(); },
        onRuntimeInitialized() {
          state.mode = 'running';
          canvas.hidden = false;
          document.getElementById('gameCover').hidden = true;
          reload.hidden = false;
          render();
          canvas.focus({preventScroll:true});
        },
        onGameExit(code) {
          if (code) { fail('Game exited with an error'); return; }
          state.mode = 'exited';
          render();
        },
        onAbort: fail,
        print: (...args) => console.log(...args),
        printErr: (...args) => console.error(...args)
      };
      for (const p of manifest.packages) await script(locate(p.script));
      await script(locate(manifest.runtime));
    } catch (error) { fail(error); }
  });
  window.addEventListener('error', event => {
    if (state.mode === 'loading' || state.mode === 'running') fail(event.message);
  });
  window.addEventListener('unhandledrejection', event => {
    if (state.mode === 'loading' || state.mode === 'running') fail(event.reason);
  });
  const page = ProjectPageCore.init({onLangChange: render});
  page.setLang(page.getLang()).then(render).finally(() => { start.disabled = false; });
})();
