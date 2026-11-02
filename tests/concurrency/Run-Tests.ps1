param([string]$VsInstallPath, [string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $false
function Invoke-TestProcess($Executable, $DllPath) {
    $process = New-Object Diagnostics.Process
    $process.StartInfo.FileName = $Executable
    $process.StartInfo.Arguments = '"' + $DllPath + '"'
    $process.StartInfo.WorkingDirectory = [IO.Path]::GetDirectoryName($Executable)
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    try {
        if (!$process.Start()) { throw "Cannot start: $Executable" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "Test process timed out after 30 seconds: $Executable"
        }
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        Set-Content -LiteralPath ($Executable + '.log') -Value $output -Encoding UTF8
        Set-Content -LiteralPath ($Executable + '.stderr.log') -Value $errors -Encoding UTF8
        Write-Host $output
        if ($errors) { Write-Host $errors }
        if ($process.ExitCode -ne 0) { throw "Test failed ($($process.ExitCode)): $Executable" }
    } finally {
        $process.Dispose()
    }
}
if (!$WorkDirectory) {
    $WorkDirectory = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))) ('Bin\concurrency-' + [Guid]::NewGuid().ToString('N'))
}
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
if (Test-Path -LiteralPath $WorkDirectory) { throw 'Choose a new output directory.' }
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
if (!$VsInstallPath) {
    $installer = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer'
    $VsInstallPath = & (Join-Path $installer 'vswhere.exe') -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
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
cl /nologo /W4 /WX /EHsc /std:c++17 /MD /utf-8 /LD "$PSScriptRoot\module_fixture.cpp" /Femodule_fixture.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /EHsc /std:c++17 /MD /utf-8 "$PSScriptRoot\module_host.cpp" /Femodule_host.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /W4 /WX /EHsc /std:c++17 /MD /utf-8 "$PSScriptRoot\state_host.cpp" /Festate_host.exe
exit /b %errorlevel%
"@
$buildPath = Join-Path $WorkDirectory 'build.cmd'
Set-Content -LiteralPath $buildPath -Value $build -Encoding ASCII
& $env:ComSpec /d /v:off /c $buildPath
if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed.' }
Invoke-TestProcess (Join-Path $WorkDirectory 'module_host.exe') (Join-Path $WorkDirectory 'module_fixture.dll')
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$msbuild = Join-Path $VsInstallPath 'MSBuild\Current\Bin\MSBuild.exe'
$output = Join-Path $WorkDirectory 'plugin\'
$objects = Join-Path $WorkDirectory 'obj\'
& $msbuild (Join-Path $repoRoot 'IPRegionMon.vcxproj') /p:Configuration=Release /p:Platform=x64 /p:IPRegionMonConcurrencyTests=true /p:PostBuildEventUseInBuild=false "/p:OutDir=$output" "/p:IntDir=$objects" /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'Test plugin compilation failed.' }
Invoke-TestProcess (Join-Path $WorkDirectory 'state_host.exe') (Join-Path $output 'IPRegionMon.dll')
Write-Host "Artifacts: $WorkDirectory"
exit 0
