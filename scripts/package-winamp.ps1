param(
    [string]$BuildDirectory = "$PSScriptRoot\..\native\build-winamp",
    [string]$Configuration = "Release",
    [string]$OutputDirectory = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path "$PSScriptRoot\..").Path
$cmake = Get-Content -Raw -LiteralPath "$repositoryRoot\native\CMakeLists.txt"
$versionMatch = [regex]::Match($cmake, 'project\(optilab_core VERSION (?<version>\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) {
    throw "Could not read the OptiLab Core version from native/CMakeLists.txt."
}
$version = $versionMatch.Groups['version'].Value
$jsfx = Get-Content -Raw -LiteralPath "$repositoryRoot\Effects\optilab_core.jsfx"
$changelog = Get-Content -Raw -LiteralPath "$repositoryRoot\CHANGELOG.md"
$escapedVersion = [regex]::Escape($version)
if ($jsfx -notmatch "(?m)^// @version $escapedVersion\r?$") {
    throw "The JSFX version does not match native version $version."
}
if ($changelog -notmatch "(?m)^## OptiLab Core v$escapedVersion\r?$") {
    throw "CHANGELOG.md has no entry for version $version."
}
$buildRoot = (Resolve-Path $BuildDirectory).Path
$dll = Join-Path $buildRoot "$Configuration\dsp_optilab_core.dll"
if (-not (Test-Path -LiteralPath $dll)) {
    throw "Winamp DSP DLL not found: $dll"
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$stage = Join-Path $outputRoot "OptiLab-Core-$version-Winamp-DSP-x86"
$archive = "$stage.zip"
$checksums = Join-Path $outputRoot "OptiLab-Core-$version-SHA256.txt"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
Remove-Item -LiteralPath $archive,$checksums -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item -LiteralPath $dll -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\native\WINAMP.md" -Destination "$stage\README.md"
Copy-Item -LiteralPath "$repositoryRoot\CHANGELOG.md","$repositoryRoot\LICENSE" -Destination $stage
$dllHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $stage "dsp_optilab_core.dll")).Hash
"$dllHash  dsp_optilab_core.dll" | Set-Content -LiteralPath (Join-Path $stage "SHA256.txt") -Encoding ascii

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $archive -CompressionLevel Optimal
if (-not (Test-Path -LiteralPath $archive)) {
    throw "Release archive was not created: $archive"
}
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath $checksums -Encoding ascii

# Standalone JSFX SHA-256 for REAPER-only users.
$jsfxSrc = Join-Path $repositoryRoot "Effects\optilab_core.jsfx"
$jsfxHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $jsfxSrc).Hash
$jsfxChecksums = Join-Path $outputRoot "OptiLab-Core-$version-JSFX-SHA256.txt"
"$jsfxHash  optilab_core.jsfx" | Set-Content -LiteralPath $jsfxChecksums -Encoding ascii

Write-Output $archive
Write-Output $checksums
Write-Output $jsfxChecksums
