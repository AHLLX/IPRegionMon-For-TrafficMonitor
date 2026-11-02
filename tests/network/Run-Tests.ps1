param([string]$VsInstallPath, [string]$WorkDirectory, [switch]$ReproOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (!$WorkDirectory) { $WorkDirectory = Join-Path $repoRoot ('Bin\network-' + [Guid]::NewGuid().ToString('N')) }
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $WorkDirectory) { throw 'Choose a new output directory.' }
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
Write-Host "Artifacts: $WorkDirectory"
if (!$VsInstallPath) {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VsInstallPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
$devCmd = Join-Path $VsInstallPath 'Common7\Tools\VsDevCmd.bat'
$installer = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer'
$build = @"
@echo off
setlocal
set "PATH=$installer;%PATH%"
call "$devCmd" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0"
cl /nologo /W4 /WX /EHsc /std:c++17 /MD /utf-8 "$PSScriptRoot\host.cpp" /Fenetwork_host.exe
exit /b %errorlevel%
"@
$buildPath = Join-Path $WorkDirectory 'build.cmd'
Set-Content -LiteralPath $buildPath -Value $build -Encoding ASCII
& $env:ComSpec /d /v:off /c $buildPath
if ($LASTEXITCODE -ne 0) { throw 'Network host compilation failed.' }
$output = Join-Path $WorkDirectory 'plugin\'
$objects = Join-Path $WorkDirectory 'obj\'
& (Join-Path $VsInstallPath 'MSBuild\Current\Bin\MSBuild.exe') (Join-Path $repoRoot 'IPRegionMon.vcxproj') /p:Configuration=Release /p:Platform=x64 /p:IPRegionMonConcurrencyTests=true /p:PostBuildEventUseInBuild=false "/p:OutDir=$output" "/p:IntDir=$objects" /fl "/flp:logfile=$WorkDirectory\build.log" /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'Test plugin compilation failed.' }
$process = New-Object Diagnostics.Process
$process.StartInfo.FileName = Join-Path $WorkDirectory 'network_host.exe'
$process.StartInfo.Arguments = '"' + (Join-Path $output 'IPRegionMon.dll') + '"' + $(if ($ReproOnly) { ' --repro' } else { '' })
$process.StartInfo.WorkingDirectory = $WorkDirectory
$process.StartInfo.UseShellExecute = $false
$process.StartInfo.CreateNoWindow = $true
$process.StartInfo.RedirectStandardOutput = $true
$process.StartInfo.RedirectStandardError = $true
try {
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit(90000)) { $process.Kill(); $process.WaitForExit(); throw 'Network test exceeded 90 seconds.' }
    $text = $stdout.GetAwaiter().GetResult()
    $errors = $stderr.GetAwaiter().GetResult()
    Set-Content -LiteralPath (Join-Path $WorkDirectory 'test.log') -Value $text -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $WorkDirectory 'stderr.log') -Value $errors -Encoding UTF8
    Write-Host $text
    if ($errors) { Write-Host $errors }
    if ($process.ExitCode -ne 0) { throw "Network test failed ($($process.ExitCode))." }
} finally { $process.Dispose() }
