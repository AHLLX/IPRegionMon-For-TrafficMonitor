param([Parameter(Mandatory=$true)][string]$NetworkArtifacts, [switch]$IncludeInternet)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$NetworkArtifacts = [IO.Path]::GetFullPath($NetworkArtifacts)
$hostExe = Join-Path $NetworkArtifacts 'network_host.exe'
$pluginDll = Join-Path $NetworkArtifacts 'plugin\IPRegionMon.dll'
if (!(Test-Path -LiteralPath $hostExe) -or !(Test-Path -LiteralPath $pluginDll)) { throw 'Run the network build first.' }
$work = Join-Path $NetworkArtifacts ('https-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
Write-Host "HTTPS artifacts: $work"
function Invoke-Probe([string]$Url, [string]$Expectation, [string]$Label) {
    $probe = [Diagnostics.Process]::new()
    $probe.StartInfo.FileName = $hostExe
    $probe.StartInfo.Arguments = '"' + $pluginDll + '" --probe "' + $Url + '" ' + $Expectation
    $probe.StartInfo.WorkingDirectory = $work
    $probe.StartInfo.UseShellExecute = $false
    $probe.StartInfo.CreateNoWindow = $true
    $probe.StartInfo.RedirectStandardOutput = $true
    $probe.StartInfo.RedirectStandardError = $true
    try {
        [void]$probe.Start()
        $stdout = $probe.StandardOutput.ReadToEndAsync()
        $stderr = $probe.StandardError.ReadToEndAsync()
        if (!$probe.WaitForExit(15000)) { $probe.Kill(); $probe.WaitForExit(); throw 'HTTPS probe exceeded 15 seconds.' }
        $text = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        Set-Content -LiteralPath (Join-Path $work ($Label + '.log')) -Value $text -Encoding UTF8
        Write-Host "$Label : $text"
        if ($probe.ExitCode -ne 0) { throw "HTTPS probe failed: $Label" }
    } finally { $probe.Dispose() }
}
$server = [Diagnostics.Process]::new()
$server.StartInfo.FileName = Join-Path $PSHOME 'powershell.exe'
if (!(Test-Path -LiteralPath $server.StartInfo.FileName)) { $server.StartInfo.FileName = Join-Path $PSHOME 'pwsh.exe' }
$server.StartInfo.Arguments = '-NoProfile -File "' + (Join-Path $PSScriptRoot 'Tls-Server.ps1') + '" -WorkDirectory "' + $work + '"'
$server.StartInfo.UseShellExecute = $false
$server.StartInfo.CreateNoWindow = $true
$server.StartInfo.RedirectStandardOutput = $true
$server.StartInfo.RedirectStandardError = $true
try {
    [void]$server.Start()
    $serverOut = $server.StandardOutput.ReadToEndAsync()
    $serverErr = $server.StandardError.ReadToEndAsync()
    $portFile = Join-Path $work 'tls-port.txt'
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while (!(Test-Path -LiteralPath $portFile) -and !$server.HasExited -and $watch.Elapsed.TotalSeconds -lt 10) { Start-Sleep -Milliseconds 50 }
    if (!(Test-Path -LiteralPath $portFile)) { throw 'TLS fixture did not start.' }
    $port = (Get-Content -LiteralPath $portFile).Trim()
    Invoke-Probe "https://127.0.0.1:$port/" 'invalid-cert' 'untrusted-local'
    if (!$server.WaitForExit(5000)) { throw 'TLS fixture did not finish.' }
    if ($server.ExitCode -ne 0) { throw 'TLS fixture failed.' }
} finally {
    if (!$server.HasExited) {
        Set-Content -LiteralPath (Join-Path $work 'stop') -Value 'stop' -Encoding ASCII
        if (!$server.WaitForExit(7000)) { $server.Kill(); $server.WaitForExit() }
    }
    Set-Content -LiteralPath (Join-Path $work 'server.log') -Value ($serverOut.GetAwaiter().GetResult() + $serverErr.GetAwaiter().GetResult()) -Encoding UTF8
    $server.Dispose()
}
if ($IncludeInternet) {
    # GET classifies remote TLS/redirect errors independently of the shorter
    # HEAD latency budget. Local untrusted-certificate checks exercise both verbs.
    $cases = @(
        @('https://www.cloudflare.com/cdn-cgi/trace', 'valid-get', 'valid-public'),
        @('https://expired.badssl.com/', 'invalid-cert-get', 'expired-public'),
        @('https://wrong.host.badssl.com/', 'invalid-cert-get', 'hostname-public'),
        @('https://self-signed.badssl.com/', 'invalid-cert-get', 'untrusted-public'),
        @('https://httpbingo.org/redirect-to?url=http%3A%2F%2Fexample.com%2F', 'no-downgrade-get', 'downgrade-public')
    )
    $failures = @()
    foreach ($case in $cases) {
        try { Invoke-Probe $case[0] $case[1] $case[2] }
        catch { $failures += $_.Exception.Message }
    }
    if ($failures.Count) { throw ($failures -join '; ') }
}
Write-Host 'PASS: HTTPS probes completed. Certificate stores were not modified.'
