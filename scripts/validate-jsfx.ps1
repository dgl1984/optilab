param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build-jsfx-validation'),
    [string]$YsfxSourceDirectory,
    [switch]$Thorough
)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$Revision='8077347ccf4115567aed81400281dca57acbb0cc'
$Script=Join-Path $Root 'Effects\optilab_core.jsfx'
if(-not(Test-Path -LiteralPath $Script)){throw 'Missing JSFX source'}
if($BuildDirectory -eq $Root){throw 'Use a separate JSFX validation build directory'}
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$Log=Join-Path $BuildDirectory 'VALIDATE_JSFX_LOG.txt'
if(Test-Path -LiteralPath $Log){Copy-Item -LiteralPath $Log -Destination (Join-Path $BuildDirectory ((Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-previous.log'))}
Start-Transcript -Path $Log -Force | Out-Null
function Run([string]$label,[string]$exe,[string[]]$arguments) {
    Write-Host "`n=== $label ==="
    $previousPreference=$ErrorActionPreference
    try {
        $ErrorActionPreference='Continue'
        & $exe @arguments 2>&1 | ForEach-Object {Write-Host $_}
        $exitCode=$LASTEXITCODE
    } finally {$ErrorActionPreference=$previousPreference}
    if($exitCode -ne 0){throw "$label failed with exit code $exitCode"}
}
try {
    $git=(Get-Command git -ErrorAction Stop).Source
    $cmake=(Get-Command cmake -ErrorAction Stop).Source
    $CustomSource=-not [string]::IsNullOrWhiteSpace($YsfxSourceDirectory)
    if(-not $CustomSource){$YsfxSourceDirectory=Join-Path $BuildDirectory 'ysfx'}
    $YsfxSourceDirectory=[IO.Path]::GetFullPath($YsfxSourceDirectory)
    if(-not(Test-Path -LiteralPath (Join-Path $YsfxSourceDirectory '.git'))) {
        if($CustomSource){throw 'The supplied ysfx directory must be an existing checkout at the pinned revision'}
        if((Test-Path -LiteralPath $YsfxSourceDirectory) -and (Get-ChildItem -LiteralPath $YsfxSourceDirectory -Force)){throw 'Refusing to initialize a nonempty dependency directory'}
        Run 'Initialize ysfx dependency' $git @('init',$YsfxSourceDirectory)
        Run 'Set official ysfx origin' $git @('-C',$YsfxSourceDirectory,'remote','add','origin','https://github.com/jpcima/ysfx.git')
        Run 'Fetch pinned ysfx revision' $git @('-C',$YsfxSourceDirectory,'fetch','--depth','1','origin',$Revision)
        Run 'Check out pinned ysfx revision' $git @('-C',$YsfxSourceDirectory,'checkout','--detach','FETCH_HEAD')
        Run 'Initialize pinned submodules' $git @('-C',$YsfxSourceDirectory,'submodule','update','--init','--recursive')
    }
    $head=(& $git -C $YsfxSourceDirectory rev-parse HEAD).Trim()
    if($LASTEXITCODE -ne 0 -or $head -ne $Revision){throw 'ysfx revision does not match the pinned runtime'}
    $dirty=& $git -C $YsfxSourceDirectory status --porcelain --untracked-files=no
    if($LASTEXITCODE -ne 0 -or $dirty){throw 'ysfx has tracked local modifications; use an unmodified pinned checkout'}
    $RuntimeBuild=Join-Path $BuildDirectory 'runtime'
    $Integration=Join-Path $Root 'tests\jsfx\ysfx-integration.cmake'
    Run 'Configure headless JSFX regression runtime' $cmake @('-S',$YsfxSourceDirectory,'-B',$RuntimeBuild,'-G','Visual Studio 17 2022','-A','Win32','-DYSFX_PLUGIN=OFF','-DYSFX_GFX=OFF','-DCMAKE_BUILD_TYPE=Release',('-DCMAKE_PROJECT_ysfx_INCLUDE='+$Integration))
    Run 'Build JSFX regression' $cmake @('--build',$RuntimeBuild,'--config','Release','--target','optilab-jsfx-regression','optilab-jsfx-legacy-state-regression','--','/m')
    $Report=Join-Path $BuildDirectory 'JSFX_RESULTS.tsv'
    $testArguments=@($Script,$Report)
    if($Thorough){$testArguments+='--thorough'}
    Run 'Run generated-signal JSFX parity and state regression' (Join-Path $RuntimeBuild 'Release\optilab-jsfx-regression.exe') $testArguments
    $count=@(Import-Csv -LiteralPath $Report -Delimiter "`t").Count
    # Includes 21 Podcast fractional switch/round-trip audio cases.
    $expected=if($Thorough){1275}else{705}
    if($count -ne $expected){throw "Incomplete JSFX report: expected $expected audio cases, found $count"}
    $LegacyReport=Join-Path $BuildDirectory 'JSFX_LEGACY_RESULTS.tsv'
    Run 'Run legacy slider-only session recall regression' (Join-Path $RuntimeBuild 'Release\optilab-jsfx-legacy-state-regression.exe') @($Script,$LegacyReport)
    $legacyCount=@(Import-Csv -LiteralPath $LegacyReport -Delimiter "`t").Count
    if($legacyCount -ne 27){throw 'Incomplete legacy JSFX session report'}
    $sourceFiles=@(Get-ChildItem -LiteralPath (Join-Path $Root 'native') -Recurse -File | Where-Object {$_.FullName -notlike '*\third_party\*' -and $_.FullName -notlike '*\generated\*'} | Sort-Object FullName | ForEach-Object {@{path=$_.FullName.Substring($Root.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
    @{status='passed';ysfxRevision=$Revision;jsfxSha256=(Get-FileHash -LiteralPath $Script -Algorithm SHA256).Hash.ToLowerInvariant();audioCases=$count;legacyAudioCases=$legacyCount;totalAudioComparisons=($count+$legacyCount);testSourceSha256=(Get-FileHash -LiteralPath (Join-Path $Root 'tests\JsfxRegression.cpp') -Algorithm SHA256).Hash.ToLowerInvariant();legacyTestSourceSha256=(Get-FileHash -LiteralPath (Join-Path $Root 'tests\JsfxLegacyStateTest.cpp') -Algorithm SHA256).Hash.ToLowerInvariant();coordinatorStates=324;numberedValues=27216;absoluteTolerance=1e-6;normalizedTolerance=1e-6;thorough=[bool]$Thorough;nativeSources=$sourceFiles;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $BuildDirectory 'JSFX_VALIDATION.json')
    Write-Host '=== JSFX PASS ==='
} catch {
    @{status='failed';error=$_.Exception.Message;finishedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $BuildDirectory 'JSFX_VALIDATION.json')
    throw
} finally {Stop-Transcript | Out-Null}
