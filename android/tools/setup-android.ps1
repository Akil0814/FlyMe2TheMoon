param([string]$SdkRoot, [switch]$AcceptLicenses)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (!$SdkRoot) { $SdkRoot = Join-Path $projectRoot 'out/android/sdk' }
$SdkRoot = [IO.Path]::GetFullPath($SdkRoot)
$manager = Join-Path $SdkRoot 'cmdline-tools/latest/bin/sdkmanager.bat'
if (!(Test-Path -LiteralPath $manager)) {
    $downloadRoot = Join-Path $projectRoot 'out/android/toolchain'
    New-Item -ItemType Directory -Force $downloadRoot | Out-Null
    $archive = Join-Path $downloadRoot 'commandlinetools.zip'
    if (!(Test-Path -LiteralPath $archive)) {
        & curl.exe -L --fail --retry 2 -o $archive 'https://dl.google.com/android/repository/commandlinetools-win-13114758_latest.zip'
        if ($LASTEXITCODE) { throw 'Android command-line tools download failed.' }
    }
    $unpacked = Join-Path $downloadRoot 'unpacked'
    Expand-Archive -LiteralPath $archive -DestinationPath $unpacked -Force
    New-Item -ItemType Directory -Force (Join-Path $SdkRoot 'cmdline-tools') | Out-Null
    Move-Item -LiteralPath (Join-Path $unpacked 'cmdline-tools') -Destination (Join-Path $SdkRoot 'cmdline-tools/latest')
}
$packages = @('platform-tools', 'platforms;android-36', 'build-tools;35.0.0', 'ndk;28.2.13676358', 'cmake;3.31.6')
if ($AcceptLicenses) {
    1..100 | ForEach-Object { 'y' } | & $manager "--sdk_root=$SdkRoot" @packages
} else { & $manager "--sdk_root=$SdkRoot" @packages }
if ($LASTEXITCODE) { throw 'Android SDK installation failed.' }
# Java properties accept forward slashes on Windows.
"sdk.dir=$($SdkRoot.Replace('\','/'))" | Set-Content -Encoding ascii (Join-Path $projectRoot 'android/local.properties')
Write-Host "Android SDK ready: $SdkRoot"
