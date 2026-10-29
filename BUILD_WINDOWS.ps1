param(
    [switch]$Incremental,
    [switch]$RequireMaintainerTests
)
Import-Module Microsoft.PowerShell.Utility -ErrorAction Stop
Import-Module Microsoft.PowerShell.Archive -ErrorAction Stop
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = [IO.Path]::GetFullPath($PSScriptRoot)
$Log = Join-Path $Root 'BUILD_WINDOWS_LOG.txt'
$Dist = Join-Path $Root 'dist'
$x64 = Join-Path $Root 'build-windows-x64'
$x86 = Join-Path $Root 'build-windows-x86'
$cmakeText = Get-Content -Raw -LiteralPath (Join-Path $Root 'native\CMakeLists.txt')
$versionMatch = [regex]::Match($cmakeText, 'project\(optilab_core VERSION (?<version>\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) { throw 'Could not read OptiLab Core version from native/CMakeLists.txt.' }
$Version = $versionMatch.Groups['version'].Value
$LogHistory = Join-Path $Root 'validation-logs'
New-Item -ItemType Directory -Force -Path $LogHistory | Out-Null
if (Test-Path -LiteralPath $Log) {
    Copy-Item -LiteralPath $Log -Destination (Join-Path $LogHistory ((Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-previous.log'))
}
Start-Transcript -Path $Log -Force | Out-Null
function Step([string]$s) { Write-Host "`n=== $s ===" }
function Run([string]$label,[string]$exe,[string[]]$commandArguments) {
    Step $label
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $exe @commandArguments 2>&1 | ForEach-Object { Write-Host $_ }
        $exitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $savedPreference }
    if ($exitCode -ne 0) { throw "$label failed with exit code $exitCode" }
}
try {
    Step 'OptiLab Core next-release validation'
    Write-Host "Source: $Root"
    $cmake=(Get-Command cmake -ErrorAction Stop).Source
    $ctest=(Get-Command ctest -ErrorAction Stop).Source
    $MaintainerPresent = Test-Path -LiteralPath (Join-Path $Root 'maintainer\CoreRegression.cpp')
    if ($RequireMaintainerTests -and -not $MaintainerPresent) {
        throw 'Maintainer validation was requested, but maintainer/CoreRegression.cpp is absent.'
    }
    Write-Host ('Maintainer validation assets: ' + $(if ($MaintainerPresent) { 'present' } else { 'not present; public tests only' }))
    Step 'Fetch verified JUCE/CLAP dependencies'
    & (Join-Path $Root 'scripts\fetch-juce-clap-deps.ps1')
    Step 'Prepare foobar2000 SDK'
    & (Join-Path $Root 'scripts\setup-foobar2000-sdk.ps1')

    Step 'Normalize future-dated source timestamps'
    $now = [DateTime]::UtcNow
    Get-ChildItem -LiteralPath (Join-Path $Root 'native') -Recurse -File |
        Where-Object { $_.LastWriteTimeUtc -gt $now } |
        ForEach-Object { $_.LastWriteTimeUtc = $now }

    if (-not $Incremental -and (Test-Path $x64)) { Remove-Item -Recurse -Force $x64 }
    Run 'Configure x64 Release' $cmake @('-S',(Join-Path $Root 'native'),'-B',$x64,'-G','Visual Studio 17 2022','-A','x64','-DOPTILAB_BUILD_JUCE_PLUGINS=ON','-DOPTILAB_BUILD_WINAMP=OFF','-DOPTILAB_BUILD_FOOBAR2000=ON','-DBUILD_TESTING=ON',('-DOPTILAB_REQUIRE_MAINTAINER_TESTS=' + $(if ($RequireMaintainerTests) { 'ON' } else { 'OFF' })))
    Run 'Build x64 formats and public tests' $cmake @('--build',$x64,'--config','Release','--target','OptiLabCoreJuce_VST3','OptiLabCoreJuce_CLAP','optilab-foobar2000','optilab-core-cli','optilab-core-public-regression','optilab-wav-file-test','optilab-winamp-pcm-test','optilab-core-behavior-regression','optilab-winamp-delivery-test','optilab-winamp-wrapper-test','optilab-juce-wrapper-test','--','/m')
    if ($MaintainerPresent) {
        Run 'Build x64 maintainer validation' $cmake @('--build',$x64,'--config','Release','--target','optilab-core-regression','--','/m')
    }
    Run 'Run x64 validation' $ctest @('--test-dir',$x64,'-C','Release','--output-on-failure')

    if (-not $Incremental -and (Test-Path $x86)) { Remove-Item -Recurse -Force $x86 }
    Run 'Configure Win32 Release' $cmake @('-S',(Join-Path $Root 'native'),'-B',$x86,'-G','Visual Studio 17 2022','-A','Win32','-DOPTILAB_BUILD_JUCE_PLUGINS=OFF','-DOPTILAB_BUILD_WINAMP=ON','-DOPTILAB_BUILD_FOOBAR2000=ON','-DBUILD_TESTING=ON',('-DOPTILAB_REQUIRE_MAINTAINER_TESTS=' + $(if ($RequireMaintainerTests) { 'ON' } else { 'OFF' })))
    Run 'Build Winamp, Win32 foobar2000, and public tests' $cmake @('--build',$x86,'--config','Release','--target','optilab-winamp-dsp','optilab-foobar2000','optilab-core-cli','optilab-core-public-regression','optilab-wav-file-test','optilab-winamp-pcm-test','optilab-core-behavior-regression','optilab-winamp-delivery-test','optilab-winamp-wrapper-test','--','/m')
    if ($MaintainerPresent) {
        Run 'Build Win32 maintainer validation' $cmake @('--build',$x86,'--config','Release','--target','optilab-core-regression','--','/m')
    }
    Run 'Run Win32 validation' $ctest @('--test-dir',$x86,'-C','Release','--output-on-failure')

    Run 'Validate JSFX companion' (Get-Command powershell.exe -ErrorAction Stop).Source @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'scripts\validate-jsfx.ps1'))

    Step 'Stage release artifacts'
    if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
    New-Item -ItemType Directory -Force -Path $Dist | Out-Null
    $vst = Get-ChildItem -Path $x64 -Recurse -Directory -Filter 'OptiLab Core.vst3' | Select-Object -First 1
    $clap = Get-ChildItem -Path $x64 -Recurse -File -Filter 'OptiLab Core.clap' | Select-Object -First 1
    $foo = Get-ChildItem -Path $x64 -Recurse -File -Filter 'foo_optilab_core.dll' | Select-Object -First 1
    $winamp = Get-ChildItem -Path $x86 -Recurse -File -Filter 'dsp_optilab_core.dll' | Select-Object -First 1
    if (-not $vst -or -not $clap -or -not $foo -or -not $winamp) { throw 'One or more release format artifacts were not found.' }
    Copy-Item $vst.FullName (Join-Path $Dist 'OptiLab Core.vst3') -Recurse
    Copy-Item $clap.FullName (Join-Path $Dist 'OptiLab Core.clap')
    Copy-Item $winamp.FullName (Join-Path $Dist 'dsp_optilab_core.dll')
    Copy-Item (Join-Path $Root 'README.md') $Dist
    Copy-Item (Join-Path $Root 'RELEASE_NOTES.md') $Dist
    Copy-Item (Join-Path $Root 'RELEASING.md') $Dist
    Copy-Item (Join-Path $Root 'CHANGELOG.md') $Dist
    Copy-Item (Join-Path $Root 'Effects') (Join-Path $Dist 'Effects') -Recurse
    Copy-Item (Join-Path $Root 'docs') (Join-Path $Dist 'docs') -Recurse
    Copy-Item (Join-Path $Root 'LICENSE') $Dist
    Copy-Item (Join-Path $Root 'NOTICE') $Dist
    Copy-Item (Join-Path $Root 'Licenses') (Join-Path $Dist 'Licenses') -Recurse
    Step 'Package dual-bitness foobar2000 component'
    & (Join-Path $Root 'scripts\package-foobar2000.ps1') -BuildDirectoryX64 $x64 -BuildDirectoryWin32 $x86 -Configuration 'Release' -OutputDirectory $Dist
    foreach ($required in @('OptiLab Core.vst3','OptiLab Core.clap','dsp_optilab_core.dll',("foo_optilab_core-{0}.fb2k-component" -f $Version),'README.md','RELEASE_NOTES.md','CHANGELOG.md','Effects\optilab_core.jsfx','docs','LICENSE','NOTICE','Licenses')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Dist $required))) { throw "Missing release payload: $required" }
    }
    $jsfxResult=Get-Content -LiteralPath (Join-Path $Root 'build-jsfx-validation\JSFX_VALIDATION.json') -Raw | ConvertFrom-Json
    $stagedJsfxHash=(Get-FileHash -LiteralPath (Join-Path $Dist 'Effects\optilab_core.jsfx') -Algorithm SHA256).Hash.ToLowerInvariant()
    if($jsfxResult.status -ne 'passed' -or $stagedJsfxHash -ne $jsfxResult.jsfxSha256){throw 'Staged JSFX does not match the passing regression result'}
    $hashLines = New-Object System.Collections.Generic.List[string]
    Get-ChildItem $Dist -Recurse -File | Sort-Object FullName | ForEach-Object {
        if ($_.Name -ne 'SHA256SUMS.txt') {
            $rel = $_.FullName.Substring($Dist.Length + 1).Replace('\','/')
            $hashLines.Add(("{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$rel))
        }
    }
    $hashLines | Set-Content -Encoding ASCII (Join-Path $Dist 'SHA256SUMS.txt')

    Step 'Create unified Windows release archive'
    $archiveTemp = Join-Path $Root ("OptiLab-Core-{0}-Windows.zip" -f $Version)
    $archiveFinal = Join-Path $Dist ("OptiLab-Core-{0}-Windows.zip" -f $Version)
    $archiveHashFile = Join-Path $Dist ("OptiLab-Core-{0}-Windows-SHA256.txt" -f $Version)
    Remove-Item -LiteralPath $archiveTemp,$archiveFinal,$archiveHashFile -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $Dist '*') -DestinationPath $archiveTemp -CompressionLevel Optimal
    Move-Item -LiteralPath $archiveTemp -Destination $archiveFinal
    $archiveHash = (Get-FileHash -LiteralPath $archiveFinal -Algorithm SHA256).Hash.ToLowerInvariant()
    ("{0}  {1}" -f $archiveHash,[IO.Path]::GetFileName($archiveFinal)) | Set-Content -Encoding ASCII $archiveHashFile

    Step 'Stage public source archive'
    & (Join-Path $Root 'scripts\package-public-source.ps1') -OutputDirectory (Join-Path $Root 'dist-source')

    Write-Host '=== PASS ==='
    Write-Host "Release candidate: $Dist"
    Write-Host ("Public source: " + (Join-Path $Root 'dist-source'))
    Write-Host "Build log: $Log"
    Write-Host 'Nothing was installed automatically.'
}
catch {
    Write-Host "=== FAIL ==="
    Write-Host $_.Exception.Message
    throw
}
finally { Stop-Transcript | Out-Null }
