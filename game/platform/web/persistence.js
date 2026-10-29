// Assets use MEMFS; only player data is persisted, independently of build versions.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  FS.mkdirTree('/player_data');
  addRunDependency('fly-progress');
  function temporary(error) {
    console.warn('Browser persistence unavailable:', error);
    try { FS.unmount('/player_data'); } catch (_) { /* Mount may have failed. */ }
    FS.mkdirTree('/player_data');
    Module['onStorageStatus']?.(false);
    removeRunDependency('fly-progress');
  }
  try {
    FS.mount(IDBFS, {autoPersist: true}, '/player_data');
    FS.syncfs(true, function (error) {
      if (error) { temporary(error); return; }
      Module['onStorageStatus']?.(true);
      removeRunDependency('fly-progress');
    });
  } catch (error) { temporary(error); }
});
