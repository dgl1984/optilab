param(
    [string]$BuildDirectory = "$PSScriptRoot\..\native\build",
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
$plugin = Join-Path $buildRoot "$Configuration\OptiLab_Core.clap"
if (-not (Test-Path -LiteralPath $plugin)) {
    throw "CLAP plug-in not found: $plugin"
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$stage = Join-Path $outputRoot "OptiLab-Core-$version-CLAP-x64"
$archive = "$stage.zip"
$checksums = Join-Path $outputRoot "OptiLab-Core-$version-CLAP-x64-SHA256.txt"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
Remove-Item -LiteralPath $archive,$checksums -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item -LiteralPath $plugin -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\native\CLAP.md" -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\native\API.md" -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\docs\CLAP_ACCESSIBILITY.md" -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\CHANGELOG.md" -Destination $stage
Copy-Item -LiteralPath "$repositoryRoot\LICENSE" -Destination $stage
$pluginHash = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $stage "OptiLab_Core.clap")).Hash
"$pluginHash  OptiLab_Core.clap" |
    Set-Content -LiteralPath (Join-Path $stage "SHA256.txt") -Encoding ascii

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $archive -CompressionLevel Optimal
if (-not (Test-Path -LiteralPath $archive)) {
    throw "Release archive was not created: $archive"
}
$archiveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
"$archiveHash  $([IO.Path]::GetFileName($archive))" |
    Set-Content -LiteralPath $checksums -Encoding ascii

Write-Output $archive
Write-Output $checksums
