param([string]$SigningDirectory = (Join-Path $env:LOCALAPPDATA 'FlyMe2TheMoon/signing'))
$ErrorActionPreference = 'Stop'
$keytool = Join-Path $env:JAVA_HOME 'bin/keytool.exe'
if (!(Test-Path -LiteralPath $keytool)) { throw 'Set JAVA_HOME to JDK 21 first.' }
$SigningDirectory = [IO.Path]::GetFullPath($SigningDirectory)
$projectRoot = [IO.Path]::GetFullPath((Split-Path (Split-Path $PSScriptRoot -Parent) -Parent))
if ($SigningDirectory.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep the release key outside the repository.'
}
New-Item -ItemType Directory -Force $SigningDirectory | Out-Null
$keyPath = Join-Path $SigningDirectory 'flyme2themoon-release.jks'
$propertiesPath = Join-Path $SigningDirectory 'signing.properties'
if ((Test-Path -LiteralPath $keyPath) -or (Test-Path -LiteralPath $propertiesPath)) {
    throw "Release signing already exists at $SigningDirectory. Reuse it; do not replace it."
}
$ownerSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
& icacls.exe $SigningDirectory /inheritance:r /grant:r "*${ownerSid}:(OI)(CI)F" | Out-Null
if ($LASTEXITCODE) { throw 'Could not restrict signing-directory access.' }
$secretBytes = New-Object byte[] 32
$generator = [Security.Cryptography.RandomNumberGenerator]::Create()
$generator.GetBytes($secretBytes); $generator.Dispose()
$password = [Convert]::ToBase64String($secretBytes)
try {
    $env:FLY_KEYTOOL_PASSWORD = $password
    & $keytool -genkeypair -keystore $keyPath -storetype PKCS12 -alias flyme2themoon -keyalg RSA -keysize 4096 -validity 10000 -dname 'CN=FlyMe2TheMoon' -storepass:env FLY_KEYTOOL_PASSWORD -keypass:env FLY_KEYTOOL_PASSWORD
    if ($LASTEXITCODE) { throw 'Release key generation failed.' }
    @("storeFile=$($keyPath.Replace('\','/'))", "storePassword=$password", 'keyAlias=flyme2themoon', "keyPassword=$password") |
        Set-Content -LiteralPath $propertiesPath -Encoding ascii
} finally { Remove-Item Env:FLY_KEYTOOL_PASSWORD -ErrorAction SilentlyContinue; $password = $null }
Write-Host "Release key and local signing configuration created in $SigningDirectory"
Write-Host 'Back up this directory securely. Future upgrades must use the same key.'
