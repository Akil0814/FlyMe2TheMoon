$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$classes = Join-Path $projectRoot 'out/android/host-tests'
New-Item -ItemType Directory -Force $classes | Out-Null
& (Join-Path $env:JAVA_HOME 'bin/javac.exe') -d $classes (Join-Path $projectRoot 'android/app/src/main/java/com/akil0814/flyme2themoon/AssetInstaller.java') (Join-Path $projectRoot 'android/tests/AssetInstallerTest.java')
if ($LASTEXITCODE) { throw 'Asset installer test compilation failed.' }
& (Join-Path $env:JAVA_HOME 'bin/java.exe') "-Djava.io.tmpdir=$classes" -cp $classes com.akil0814.flyme2themoon.AssetInstallerTest
if ($LASTEXITCODE) { throw 'Asset installer tests failed.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $visualStudio = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($visualStudio) {
        & (Join-Path $PSScriptRoot 'test-touch.cmd') $visualStudio $classes
        if ($LASTEXITCODE) { throw 'Touch pointer tests failed.' }
    } else { Write-Warning 'MSVC unavailable: native host touch tests skipped.' }
} else { Write-Warning 'MSVC unavailable: native host touch tests skipped.' }
