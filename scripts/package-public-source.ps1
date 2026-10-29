param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\dist-source')
)

$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$Stage = Join-Path $OutputDirectory 'OptiLab-Core'

$cmakeText = Get-Content -Raw -LiteralPath (Join-Path $Root 'native\CMakeLists.txt')
$versionMatch = [regex]::Match($cmakeText, 'project\(optilab_core VERSION (?<version>\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) { throw 'Could not read OptiLab Core version.' }
$Version = $versionMatch.Groups['version'].Value

# Never remove source, its parents, copied source folders, or unrelated output files.
$outputPrefix = $OutputDirectory.TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
if ($Root.Equals($OutputDirectory, [StringComparison]::OrdinalIgnoreCase) -or
    $Root.StartsWith($outputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Public-source output cannot be the source root or one of its parents.'
}
foreach ($folder in @('native','Effects','docs','Licenses','scripts','tests','maintainer')) {
    $inputPath = Join-Path $Root $folder
    if ($OutputDirectory.Equals($inputPath, [StringComparison]::OrdinalIgnoreCase) -or
        $OutputDirectory.StartsWith($inputPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Public-source output cannot be inside a source input directory.'
    }
}
if (Test-Path -LiteralPath $OutputDirectory) {
    if (Get-ChildItem -LiteralPath $OutputDirectory -Force | Where-Object {
        $_.Name -ne 'OptiLab-Core' -and
        $_.Name -notmatch '^OptiLab-Core-\d+\.\d+\.\d+-(?:Source(?:-SHA256\.txt|\.zip)|public-source(?:-SHA256\.txt|\.zip))$'
    }) {
        throw 'Public-source output contains unrelated files; select an empty output directory.'
    }
    Remove-Item -LiteralPath $OutputDirectory -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

$topFiles = @(
    '.gitattributes','.gitignore','BUILD_WINDOWS.bat','BUILD_WINDOWS.ps1','VALIDATE_JSFX.bat',
    'CHANGELOG.md','CONTRIBUTING.md','RELEASING.md','LICENSE','NOTICE','README.md','RELEASE_NOTES.md','TESTING.md'
)
foreach ($name in $topFiles) {
    $source = Join-Path $Root $name
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing public-source file: $name" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $Stage $name)
}

foreach ($folder in @('Effects','docs','Licenses','scripts','tests')) {
    $source = Join-Path $Root $folder
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing public-source folder: $folder" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $Stage $folder) -Recurse
}

# Copy native source without fetched dependencies or generated/build state.
$nativeOut = Join-Path $Stage 'native'
New-Item -ItemType Directory -Force -Path $nativeOut | Out-Null
Get-ChildItem -LiteralPath (Join-Path $Root 'native') -Recurse -File | ForEach-Object {
    $relative = $_.FullName.Substring((Join-Path $Root 'native').Length + 1)
    if ($relative -notlike 'third_party\*' -and $relative -notlike 'generated\*') {
        $destination = Join-Path $nativeOut $relative
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $destination
    }
}

$forbiddenNames = @('maintainer','.git','.cache','dist','dist-source','dist-private','validation-logs','reference-jsfx','build-windows-x64','build-windows-x86','build-jsfx-validation','LegacyParameterIds.h','PRIVATE_PARAMETER_MAP.json','PRIVATE_LEGACY_ALIAS_MAP.json')
foreach ($name in $forbiddenNames) {
    if (Get-ChildItem -LiteralPath $Stage -Recurse -Force | Where-Object { $_.Name -eq $name }) {
        throw "Forbidden public-source payload found: $name"
    }
}
if (Get-ChildItem -LiteralPath $Stage -Recurse -File | Where-Object {
        $_.Extension -in @('.dll','.exe','.pdb','.lib','.obj','.zip') -or $_.Name -eq 'BUILD_WINDOWS_LOG.txt'
    }) {
    throw 'Generated binary, archive, or build transcript found in public-source stage.'
}

foreach ($requiredTest in @('CorePublicRegression.cpp','WavFileTest.cpp','WinampPcmTest.cpp','CoreBehaviorRegression.cpp','WinampDeliveryTest.cpp','WinampWrapperTest.cpp','JuceWrapperTest.cpp','JsfxRegression.cpp','JsfxLegacyStateTest.cpp')) {
    if (-not (Test-Path -LiteralPath (Join-Path $Stage ('tests\' + $requiredTest)))) {
        throw "Missing public regression test: $requiredTest"
    }
}

$forbiddenText = @(
    'BeefyStreamBroadcast',
    'EventMultibandLimiterClipper',
    'SmoothMasteringLimiter',
    'APPENDED FINAL HYBRID',
    'Candidate-distortion burden',
    'Orban algorithm',
    'JSFX uses the current'
)
foreach ($pattern in $forbiddenText) {
    $hit = Get-ChildItem -LiteralPath $Stage -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp','.h','.def','.md','.txt','.jsfx') } |
        Select-String -SimpleMatch -Pattern $pattern -List
    if ($hit) { throw "Development-only source marker found in public package: $pattern" }
}

$forbiddenEngineText = @('OptiLabProducer','ProducerCore','Service+')
foreach ($pattern in $forbiddenEngineText) {
    $hit = Get-ChildItem -LiteralPath (Join-Path $Stage 'native') -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp','.h','.def','.txt') } |
        Select-String -SimpleMatch -Pattern $pattern -List
    if ($hit) { throw "Non-Core engine marker found in public native source: $pattern" }
}

# The published catalogue is runtime-only. Core tuning must never be folded back
# into this registry, because doing so recreates a generic hidden control surface.
$catalogue = Get-Content -Raw -LiteralPath (Join-Path $Stage 'native\engine\OptiLabParameters.def')
if ($catalogue -match 'Exposure::Internal') { throw 'Core-private tuning leaked into the public parameter catalogue.' }
$entries = [regex]::Matches($catalogue, '(?m)^OP_PARAMETER\([^,]+,\s*(p\d{3}),\s*[^,]+,\s*"(p\d{3})"')
if ($entries.Count -ne 48) { throw 'Expected 48 public runtime parameter entries.' }
$seen = @{}
foreach ($entry in $entries) {
    if ($entry.Groups[1].Value -ne $entry.Groups[2].Value) { throw 'Public parameter member/key mismatch.' }
    if ($seen.ContainsKey($entry.Groups[1].Value)) { throw 'Duplicate public parameter ID.' }
    $seen[$entry.Groups[1].Value] = $true
}

$legacyInternalIds = @('p005','p007','p008','p011','p012','p013','p014','p015','p024','p025','p026','p045','p046','p047','p051','p056','p057','p058','p059','p060','p066','p067','p068','p070','p071','p072','p073','p074','p075','p076','p077','p078','p081','p082','p083','p084')
foreach ($id in $legacyInternalIds) {
    $hit = Get-ChildItem -LiteralPath (Join-Path $Stage 'native') -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp','.h','.def') } | Select-String -SimpleMatch -Pattern $id -List
    if ($hit) { throw "Retired internal catalogue ID leaked into native source: $id" }
    $jsfx = Select-String -LiteralPath (Join-Path $Stage 'Effects\optilab_core.jsfx') -SimpleMatch -Pattern $id -List
    if ($jsfx) { throw "Retired internal catalogue ID leaked into JSFX source: $id" }
}

$hashLines = New-Object System.Collections.Generic.List[string]
Get-ChildItem -LiteralPath $Stage -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($Stage.Length + 1).Replace('\','/')
    $hashLines.Add(("{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative))
}
$hashLines | Set-Content -Encoding ASCII (Join-Path $Stage 'SOURCE_MANIFEST.sha256')

$archive = Join-Path $OutputDirectory ("OptiLab-Core-{0}-Source.zip" -f $Version)
Compress-Archive -Path $Stage -DestinationPath $archive -CompressionLevel Optimal
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
("{0}  {1}" -f $archiveHash,[IO.Path]::GetFileName($archive)) |
    Set-Content -Encoding ASCII (Join-Path $OutputDirectory ("OptiLab-Core-{0}-Source-SHA256.txt" -f $Version))

Write-Host "Public source staged at: $Stage"
Write-Host "Archive: $archive"
