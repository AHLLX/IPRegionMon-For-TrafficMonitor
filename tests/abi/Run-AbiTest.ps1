param(
    [string]$ReferenceHeader,
    [string]$LegacyHeader,
    [string]$PluginDll,
    [string]$WorkDirectory,
    [string]$VsInstallPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$referenceCommit = '930f17533d6098989ebad62f210aa97d75ef174b'
$legacyCommit = '935e6a8fcea923a9a038fff736061af2066d34b9'

function Copy-PinnedHeader($InputPath, $Destination, $Url, $Hash) {
    if ($InputPath) {
        Copy-Item -LiteralPath $InputPath -Destination $Destination
    } else {
        Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing -TimeoutSec 30
    }
    if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -cne $Hash) {
        throw "Header checksum mismatch: $Destination"
    }
}

function Read-NormalizedHeader($Path) {
    # Git may check out CRLF on Windows; all other content must match upstream.
    return ([IO.File]::ReadAllText($Path)).Replace(
        ([string][char]13 + [char]10), [string][char]10)
}

if (!$WorkDirectory) {
    $WorkDirectory = Join-Path $env:TEMP ('IPRegionMon-abi-' + [Guid]::NewGuid().ToString('N'))
}
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $WorkDirectory) {
    if (@(Get-ChildItem -LiteralPath $WorkDirectory -Force).Count -ne 0) {
        throw 'WorkDirectory must be new or empty; existing files will not be overwritten.'
    }
} else {
    New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
}
Write-Host "Artifacts: $WorkDirectory"

$referenceDir = Join-Path $WorkDirectory 'reference'
$legacyDir = Join-Path $WorkDirectory 'legacy'
New-Item -ItemType Directory -Path $referenceDir, $legacyDir | Out-Null
$referencePath = Join-Path $referenceDir 'PluginInterface.h'
$legacyPath = Join-Path $legacyDir 'PluginInterface.h'
$referenceUrl = "https://raw.githubusercontent.com/zhongyang219/TrafficMonitor/$referenceCommit/include/PluginInterface.h"
$legacyUrl = "https://raw.githubusercontent.com/AHLLX/IPRegionMon-For-TrafficMonitor/$legacyCommit/include/PluginInterface.h"
Copy-PinnedHeader $ReferenceHeader $referencePath $referenceUrl 'E11A2C07B7E49483779ED9A210702A8307AC336ABB879F62BFBD3AAF3342289C'
Copy-PinnedHeader $LegacyHeader $legacyPath $legacyUrl 'ABE0564B63849886E03D21750CACB0DD19C75D1EB1BD07C7DA40629436345DE7'

$candidateText = Read-NormalizedHeader (Join-Path $repoRoot 'include\PluginInterface.h')
$referenceText = Read-NormalizedHeader $referencePath
$headerMatches = $candidateText -ceq $referenceText
Write-Host "Header matches pinned upstream (ignoring CRLF): $headerMatches"
foreach ($className in @('IPluginItem', 'ITMPlugin', 'ITrafficMonitor', 'IPluginDrawer')) {
    $pattern = '(?s)class ' + $className + '\s*\{(.*?)\r?\n\};'
    $candidateBody = [regex]::Match($candidateText, $pattern).Groups[1].Value
    $referenceBody = [regex]::Match($referenceText, $pattern).Groups[1].Value
    $methodPattern = '(?m)^\s*virtual[^\r\n]+'
    $candidateMethods = @([regex]::Matches($candidateBody, $methodPattern) |
        ForEach-Object { $_.Value.Trim() })
    $referenceMethods = @([regex]::Matches($referenceBody, $methodPattern) |
        ForEach-Object { $_.Value.Trim() })
    $methodsMatch = $candidateMethods.Count -gt 0 -and
        ($candidateMethods -join '|') -ceq ($referenceMethods -join '|')
    Write-Host ("{0}: {1}/{2} slots, same order/signatures: {3}" -f
        $className, $candidateMethods.Count, $referenceMethods.Count, $methodsMatch)
}

if (!$VsInstallPath) {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VsInstallPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or !$VsInstallPath) {
        throw 'MSVC x64/x86 tools were not found. Supply -VsInstallPath.'
    }
}
$devCmd = Join-Path $VsInstallPath 'Common7\Tools\VsDevCmd.bat'
if (!(Test-Path -LiteralPath $devCmd)) { throw "Not found: $devCmd" }
$fixture = Join-Path $PSScriptRoot 'fixture.cpp'
$hostSource = Join-Path $PSScriptRoot 'host.cpp'
$candidateDir = Join-Path $repoRoot 'include'
$installerDir = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer'
foreach ($path in @($devCmd, $fixture, $hostSource, $candidateDir, $installerDir, $WorkDirectory)) {
    if ($path -match '["%\r\n]') { throw "Unsupported character in build path: $path" }
}
$buildScript = @"
@echo off
setlocal
set "PATH=$installerDir;%PATH%"
call "$devCmd" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
cl /nologo /W4 /WX /EHsc /std:c++17 /Od /MD /utf-8 /LD "$fixture" /external:I"$candidateDir" /external:W0 /Focandidate.obj /Fecandidate.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /EHsc /std:c++17 /Od /MD /utf-8 /LD "$fixture" /external:Ilegacy /external:W0 /Folegacy.obj /Felegacy.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /EHsc /std:c++17 /Od /MD /utf-8 "$hostSource" /external:Ireference /external:W0 /Fohost.obj /Fehost.exe
exit /b %errorlevel%
"@
$buildPath = Join-Path $WorkDirectory 'build-fixtures.cmd'
Set-Content -LiteralPath $buildPath -Value $buildScript -Encoding ASCII
$compileOutput = & $env:ComSpec /d /v:off /c $buildPath 2>&1
$compileExit = $LASTEXITCODE
$compileOutput | Tee-Object -FilePath (Join-Path $WorkDirectory 'compile.log') | Out-Host
if ($compileExit -ne 0) { throw "Fixture build failed: $compileExit" }

$hostExe = Join-Path $WorkDirectory 'host.exe'
$probes = @(
    @{ Name = 'candidate'; Dll = (Join-Path $WorkDirectory 'candidate.dll'); Mode = 'fixture'; Expected = @(0) },
    # Undefined behavior may fault or return the wrong value; it must never pass.
    @{ Name = 'legacy-negative-control'; Dll = (Join-Path $WorkDirectory 'legacy.dll'); Mode = 'fixture'; Expected = @(10, 12) }
)
if ($PluginDll) {
    $probes += @{ Name = 'IPRegionMon'; Dll = (Resolve-Path -LiteralPath $PluginDll).Path; Mode = 'plugin'; Expected = @(0) }
}
$allPassed = $headerMatches
foreach ($probe in $probes) {
    Write-Host "Probe: $($probe.Name)"
    Push-Location -LiteralPath $WorkDirectory
    try {
        $output = & $hostExe $probe.Dll $probe.Mode 2>&1
        $probeExit = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    $output | Tee-Object -FilePath (Join-Path $WorkDirectory ($probe.Name + '.log')) | Out-Host
    Write-Host "Exit: $probeExit; expected: $($probe.Expected -join ', ')"
    if ($probeExit -notin $probe.Expected) { $allPassed = $false }
}
if (!$allPassed) { throw "ABI verification failed. Artifacts: $WorkDirectory" }
Write-Host 'PASS: pinned header, positive ABI probe and legacy negative control.'
exit 0
