param([ValidateSet('Debug','Release','All')][string]$Variant = 'All', [string]$SigningProperties = (Join-Path $env:LOCALAPPDATA 'FlyMe2TheMoon/signing/signing.properties'))
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$androidRoot = Join-Path $projectRoot 'android'
if (!(Test-Path -LiteralPath (Join-Path $androidRoot 'local.properties'))) { throw 'Run android/tools/setup-android.ps1 first.' }
if ($Variant -ne 'Debug' -and !(Test-Path -LiteralPath $SigningProperties)) { throw 'Run new-release-key.ps1 or specify -SigningProperties.' }
if (!$env:GRADLE_USER_HOME) { $env:GRADLE_USER_HOME = Join-Path $projectRoot 'out/android/gradle-cache' }
$tasks = switch ($Variant) { 'Debug' { @('assembleDebug') } 'Release' { @('assembleRelease') } 'All' { @('assembleDebug','assembleRelease') } }
Push-Location $androidRoot
try {
    & ./gradlew.bat @tasks "-PflySigningProperties=$SigningProperties" --console=plain --no-daemon
    if ($LASTEXITCODE) { throw 'Android build failed.' }
} finally { Pop-Location }
$destination = Join-Path $projectRoot 'out/android/dist'
New-Item -ItemType Directory -Force $destination | Out-Null
foreach ($kind in @('debug','release')) {
    if ($Variant -ne 'All' -and $Variant.ToLowerInvariant() -ne $kind) { continue }
    $source = Join-Path $androidRoot "app/build/outputs/apk/$kind/app-$kind.apk"
    $target = Join-Path $destination "FlyMe2TheMoon-1.1.0-arm64-$kind.apk"
    Copy-Item -LiteralPath $source -Destination $target -Force
    & (Join-Path $PSScriptRoot 'verify-apk.ps1') -Apk $target
}
$symbols = Join-Path $androidRoot 'app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip'
if ($Variant -ne 'Debug' -and (Test-Path -LiteralPath $symbols)) {
    Copy-Item -LiteralPath $symbols -Destination (Join-Path $destination 'FlyMe2TheMoon-1.1.0-native-symbols.zip') -Force
} elseif ($Variant -ne 'Debug') {
    # AGP versions that only zip symbols for bundle tasks still retain unstripped
    # linked libraries. Keep these separately from the stripped release APK.
    $symbolRoot = Join-Path $androidRoot 'app/build/intermediates/cxx/RelWithDebInfo'
    $native = @(Get-ChildItem -LiteralPath $symbolRoot -Recurse -Filter libmain.so | Where-Object { $_.FullName -match '[\\/]obj[\\/]arm64-v8a[\\/]' })
    if ($native.Count -ne 1) { throw 'Could not identify the current Release native symbols.' }
    Compress-Archive -Path (Join-Path $native[0].Directory.FullName '*.so') -DestinationPath (Join-Path $destination 'FlyMe2TheMoon-1.1.0-native-symbols.zip') -Force
}
Write-Host "Android artifacts: $destination"
