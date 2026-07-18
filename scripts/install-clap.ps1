param(
    [string]$BuildDirectory = "$PSScriptRoot\..\native\build",
    [string]$Configuration = "Release",
    [string]$DestinationDirectory = "$env:LOCALAPPDATA\Programs\Common\CLAP"
)

$ErrorActionPreference = "Stop"
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
$source = Join-Path $buildRoot "$Configuration\OptiLab_Core.clap"
if (-not (Test-Path -LiteralPath $source)) {
    throw "CLAP build not found: $source. Build the vs2022-x64 Release preset first."
}

$destinationRoot = [IO.Path]::GetFullPath($DestinationDirectory)
New-Item -ItemType Directory -Force -Path $destinationRoot | Out-Null
$destination = Join-Path $destinationRoot "OptiLab_Core.clap"
Copy-Item -LiteralPath $source -Destination $destination -Force

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
$destinationHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash
if ($sourceHash -ne $destinationHash) {
    throw "Installed CLAP hash does not match the build output."
}

Write-Output "Installed: $destination"
Write-Output "SHA-256: $destinationHash"
