param([switch]$Force = $false)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$Native = Join-Path $Root 'native'
$ThirdParty = Join-Path $Native 'third_party'
$Cache = Join-Path $Root '.cache'
$JuceDir = Join-Path $ThirdParty 'JUCE'
$CjeDir = Join-Path $ThirdParty 'clap-juce-extensions'
$JuceUrl = 'https://github.com/juce-framework/JUCE/releases/download/8.0.12/juce-8.0.12-windows.zip'
$JuceSha = '15b4ed8302127138dfee98e61a2b5ae1481b39649b139e02e77bfc29462b7efb'
$CjeCommit = '55525c9858d4b25687be7759a5e0f70eccef218e'
New-Item -ItemType Directory -Force -Path $ThirdParty,$Cache | Out-Null

if ($Force -and (Test-Path $JuceDir)) { Remove-Item -Recurse -Force $JuceDir }
$zip = Join-Path $Cache 'juce-8.0.12-windows.zip'
if (-not (Test-Path $zip)) { Invoke-WebRequest -Uri $JuceUrl -OutFile $zip }
$actual = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant()
if ($actual -ne $JuceSha) { throw "JUCE archive hash mismatch: $actual" }
if (-not (Test-Path (Join-Path $JuceDir 'CMakeLists.txt'))) {
    $tmp = Join-Path $Cache 'juce-extract'
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    Expand-Archive -LiteralPath $zip -DestinationPath $tmp -Force
    $candidate = Get-ChildItem -Path $tmp -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'CMakeLists.txt') } | Select-Object -First 1
    if (-not $candidate) { throw 'JUCE archive layout was not recognized.' }
    if (Test-Path $JuceDir) { Remove-Item -Recurse -Force $JuceDir }
    Move-Item $candidate.FullName $JuceDir
    Remove-Item -Recurse -Force $tmp
}
$actual = (Get-FileHash -Algorithm SHA256 (Join-Path $Cache 'juce-8.0.12-windows.zip')).Hash.ToLowerInvariant()
if ($actual -ne $JuceSha) { throw 'Cached JUCE archive failed verification.' }
Write-Host "JUCE 8.0.12 verified: $JuceDir"

$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { throw 'Git is required to fetch the pinned CLAP JUCE wrapper.' }
if ($Force -and (Test-Path $CjeDir)) { Remove-Item -Recurse -Force $CjeDir }
if (-not (Test-Path (Join-Path $CjeDir '.git'))) {
    if (Test-Path $CjeDir) { Remove-Item -Recurse -Force $CjeDir }
    & $git.Source clone https://github.com/free-audio/clap-juce-extensions.git $CjeDir
    if ($LASTEXITCODE -ne 0) { throw 'Could not clone clap-juce-extensions.' }
}
Push-Location $CjeDir
try {
    & $git.Source fetch --all --tags
    if ($LASTEXITCODE -ne 0) { throw 'Could not update clap-juce-extensions.' }
    & $git.Source checkout --detach $CjeCommit
    if ($LASTEXITCODE -ne 0) { throw 'Could not check out pinned clap-juce-extensions commit.' }
    & $git.Source submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize pinned CLAP submodules.' }
    $head = (& $git.Source rev-parse HEAD).Trim()
    if ($head -ne $CjeCommit) { throw "CLAP wrapper commit mismatch: $head" }
}
finally { Pop-Location }
Write-Host "clap-juce-extensions verified: $CjeCommit"
