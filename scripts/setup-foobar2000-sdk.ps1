param(
    [string]$Destination = "$PSScriptRoot\..\native\third_party\foobar2000-sdk"
)

$ErrorActionPreference = "Stop"
$sdkVersion = "2026-10-01"
$expectedHash = "D4C55077336FAE81BF8DF0259B5B2748FA45EA84132C656EAD93EB123CBCDC26"
$downloadUrl = "https://www.foobar2000.org/downloads/SDK-$sdkVersion.7z"
$destinationPath = [IO.Path]::GetFullPath($Destination)

$licensePath = Join-Path $destinationPath "sdk-license.txt"
$headerPath = Join-Path $destinationPath "foobar2000\SDK\foobar2000.h"
$versionMarker = Join-Path $destinationPath ".optilab-sdk-version"
$expectedMarker = "$sdkVersion $expectedHash"
$markerMatches = $false
if (Test-Path -LiteralPath $versionMarker) {
    $markerMatches = ((Get-Content -LiteralPath $versionMarker -Raw).Trim() -eq $expectedMarker)
}
if ((Test-Path -LiteralPath $licensePath) -and
    (Test-Path -LiteralPath $headerPath) -and $markerMatches) {
    Write-Output "foobar2000 SDK $sdkVersion is already verified at $destinationPath"
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
        Write-Output "Replacing stale or unverified foobar2000 SDK at $destinationPath"
        Remove-Item -LiteralPath $destinationPath -Recurse -Force
    }
    Move-Item -LiteralPath $extractRoot -Destination $destinationPath
    if (-not (Test-Path -LiteralPath $licensePath) -or
        -not (Test-Path -LiteralPath $headerPath)) {
        throw "The foobar2000 SDK installation did not produce the expected files."
    }
    Set-Content -LiteralPath $versionMarker -Encoding ASCII -NoNewline -Value $expectedMarker
    if (((Get-Content -LiteralPath $versionMarker -Raw).Trim()) -ne $expectedMarker) {
        throw "The foobar2000 SDK version marker could not be verified."
    }
    Write-Output "Installed and verified foobar2000 SDK $sdkVersion at $destinationPath"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
