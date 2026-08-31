param(
    [string]$BuildDirectoryX64 = "$PSScriptRoot\..\native\build-foobar2000-x64",
    [string]$BuildDirectoryWin32 = "$PSScriptRoot\..\native\build-foobar2000-win32",
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

function Resolve-Component([string]$buildDirectory) {
    $buildRoot = [IO.Path]::GetFullPath($buildDirectory)
    $multiConfigPath = Join-Path $buildRoot "$Configuration\foo_optilab_core.dll"
    if (Test-Path -LiteralPath $multiConfigPath) {
        return $multiConfigPath
    }
    return Join-Path $buildRoot "foo_optilab_core.dll"
}

$x64Dll = Resolve-Component $BuildDirectoryX64
$win32Dll = Resolve-Component $BuildDirectoryWin32
foreach ($component in @($x64Dll, $win32Dll)) {
    if (-not (Test-Path -LiteralPath $component)) {
        throw "foobar2000 component not found: $component"
    }
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$stage = Join-Path $outputRoot "foo_optilab_core-$version-stage"
$package = Join-Path $outputRoot "foo_optilab_core-$version.fb2k-component"
$checksums = Join-Path $outputRoot "foo_optilab_core-$version-SHA256.txt"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
Remove-Item -LiteralPath $package,$checksums -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $stage "x64") -Force | Out-Null
Copy-Item -LiteralPath $win32Dll -Destination (Join-Path $stage "foo_optilab_core.dll")
Copy-Item -LiteralPath $x64Dll -Destination (Join-Path $stage "x64\foo_optilab_core.dll")
Copy-Item -LiteralPath "$repositoryRoot\LICENSE" -Destination (Join-Path $stage "LICENSE.txt")
Copy-Item -LiteralPath "$repositoryRoot\NOTICE" -Destination $stage
$sdkLicense = Join-Path $repositoryRoot "native\third_party\foobar2000-sdk\sdk-license.txt"
if (-not (Test-Path -LiteralPath $sdkLicense)) {
    throw "foobar2000 SDK license not found: $sdkLicense"
}
Copy-Item -LiteralPath $sdkLicense -Destination (Join-Path $stage "foobar2000-sdk-license.txt")

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $package -CompressionLevel Optimal
$packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $package).Hash
"$packageHash  $([IO.Path]::GetFileName($package))" |
    Set-Content -LiteralPath $checksums -Encoding ascii
Remove-Item -LiteralPath $stage -Recurse -Force

Write-Output $package
Write-Output $checksums
