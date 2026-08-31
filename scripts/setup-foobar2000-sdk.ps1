param(
    [string]$Destination = "$PSScriptRoot\..\native\third_party\foobar2000-sdk"
)

$ErrorActionPreference = "Stop"
$sdkVersion = "2025-03-07"
$expectedHash = "CCDA3C5840E66E0E28A7E4FE36407C4E78581AA30C40C362A188FCBAAE799A3E"
$downloadUrl = "https://www.foobar2000.org/downloads/SDK-$sdkVersion.7z"
$destinationPath = [IO.Path]::GetFullPath($Destination)

$licensePath = Join-Path $destinationPath "sdk-license.txt"
$headerPath = Join-Path $destinationPath "foobar2000\SDK\foobar2000.h"
if ((Test-Path -LiteralPath $licensePath) -and
    (Test-Path -LiteralPath $headerPath)) {
    Write-Output "foobar2000 SDK is already installed at $destinationPath"
    exit 0
}

$sevenZip = Get-Command 7z -ErrorAction SilentlyContinue
if (-not $sevenZip) {
    throw "7-Zip is required. Install 7-Zip and ensure the 7z command is on PATH."
}

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) "optilab-foobar2000-sdk-$([guid]::NewGuid())"
$archive = Join-Path $temporaryRoot "SDK-$sdkVersion.7z"
$extractRoot = Join-Path $temporaryRoot "extract"
New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null

try {
    Invoke-WebRequest -Uri $downloadUrl -OutFile $archive
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    if ($actualHash -ne $expectedHash) {
        throw "foobar2000 SDK archive hash mismatch. Expected $expectedHash, got $actualHash."
    }
    & $sevenZip.Source x $archive "-o$extractRoot" -y | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip could not extract the foobar2000 SDK archive."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $extractRoot "foobar2000\SDK\foobar2000.h"))) {
        throw "The extracted archive does not have the expected foobar2000 SDK layout."
    }
    New-Item -ItemType Directory -Path (Split-Path $destinationPath -Parent) -Force | Out-Null
    if (Test-Path -LiteralPath $destinationPath) {
        Copy-Item -Path (Join-Path $extractRoot "*") -Destination $destinationPath `
            -Recurse -Force
    }
    else {
        Move-Item -LiteralPath $extractRoot -Destination $destinationPath
    }
    if (-not (Test-Path -LiteralPath $licensePath) -or
        -not (Test-Path -LiteralPath $headerPath)) {
        throw "The foobar2000 SDK installation did not produce the expected files."
    }
    Write-Output "Installed foobar2000 SDK $sdkVersion at $destinationPath"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
