param([Parameter(Mandatory)][string]$Apk)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sdkLine = Get-Content (Join-Path $projectRoot 'android/local.properties') | Where-Object { $_ -match '^sdk.dir=' } | Select-Object -First 1
$sdk = $sdkLine.Substring(8).Replace('\\', '\').Replace('\:', ':')
$tools = Join-Path $sdk 'build-tools/35.0.0'
& (Join-Path $tools 'apksigner.bat') verify --verbose --print-certs $Apk | Tee-Object -FilePath "$Apk.signature.txt"
if ($LASTEXITCODE) { throw 'APK signature verification failed.' }
& (Join-Path $tools 'zipalign.exe') -c -P 16 4 $Apk
if ($LASTEXITCODE) { throw 'APK zip alignment failed.' }
$badging = & (Join-Path $tools 'aapt.exe') dump badging $Apk
if ($LASTEXITCODE) { throw 'APK metadata inspection failed.' }
$badging | Set-Content -Encoding utf8 "$Apk.metadata.txt"
if (!($badging -match "package: name='com.akil0814.flyme2themoon' versionCode='2' versionName='1.0.1'")) { throw 'Unexpected APK identity.' }
if (!($badging -match "native-code: 'arm64-v8a'$") -or !($badging -match "sdkVersion:'26'")) { throw 'Unexpected APK ABI or minimum SDK.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Apk))
try {
    foreach ($required in @('assets/game-assets.zip','assets/game-assets.json','lib/arm64-v8a/libmain.so','lib/arm64-v8a/libSDL3.so','lib/arm64-v8a/libc++_shared.so')) {
        if (!$archive.GetEntry($required)) { throw "Missing APK entry: $required" }
    }
    $reader = [IO.StreamReader]::new($archive.GetEntry('assets/game-assets.json').Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    $packStream = [IO.MemoryStream]::new()
    $packed = $archive.GetEntry('assets/game-assets.zip').Open()
    try { $packed.CopyTo($packStream) } finally { $packed.Dispose() }
    $packStream.Position = 0
    $pack = [IO.Compression.ZipArchive]::new($packStream, [IO.Compression.ZipArchiveMode]::Read)
    try {
        if ($pack.Entries.Count -ne $manifest.files.Count) { throw 'Resource count mismatch.' }
        $totalBytes = 0L
        foreach ($item in $manifest.files) {
            $entry = $pack.GetEntry($item.path)
            if (!$entry -or $entry.Length -ne $item.size) { throw "Resource size mismatch: $($item.path)" }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $actual = [Convert]::ToHexString($sha.ComputeHash($stream)).ToLowerInvariant() }
            finally { $sha.Dispose(); $stream.Dispose() }
            if ($actual -ne $item.sha256) { throw "Resource checksum mismatch: $($item.path)" }
            $totalBytes += $entry.Length
        }
        if ($totalBytes -ne $manifest.totalBytes) { throw 'Total resource size mismatch.' }
        foreach ($required in @('assets/.elysia_root','assets/content_registry.json','assets/configs/levels/001.json','assets/configs/levels/060.json')) {
            if (!$pack.GetEntry($required)) { throw "Missing game resource: $required" }
        }
        Write-Host "Resource archive verified: $($manifest.files.Count) files, $totalBytes bytes"
    } finally { $pack.Dispose(); $packStream.Dispose() }
    $objdump = Join-Path $sdk 'ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-objdump.exe'
    $scratch = Join-Path $projectRoot 'out/android/verify'
    New-Item -ItemType Directory -Force $scratch | Out-Null
    foreach ($entry in $archive.Entries | Where-Object { $_.FullName -like 'lib/*/*.so' }) {
        $library = Join-Path $scratch $entry.Name
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $library, $true)
        $headers = & $objdump -p $library
        if ($LASTEXITCODE) { throw "Cannot inspect $($entry.Name)" }
        $loads = @($headers | Where-Object { $_ -match '^\s*LOAD ' })
        if (!$loads.Count) { throw "No load segments in $($entry.Name)" }
        foreach ($load in $loads) {
            if ($load -notmatch 'align 2\*\*(\d+)' -or [int]$Matches[1] -lt 14) { throw "Native library is not 16 KiB aligned: $($entry.Name)" }
        }
    }
} finally { $archive.Dispose() }
$hash = (Get-FileHash -LiteralPath $Apk -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($Apk))" | Set-Content -Encoding ascii "$Apk.sha256"
Write-Host "Verified APK: $Apk"
