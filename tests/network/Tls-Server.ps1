param([Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference = 'Stop'
$rsa = [Security.Cryptography.RSACng]::new(2048)
$request = [Security.Cryptography.X509Certificates.CertificateRequest]::new(
    'CN=localhost', $rsa, [Security.Cryptography.HashAlgorithmName]::SHA256,
    [Security.Cryptography.RSASignaturePadding]::Pkcs1)
$san = [Security.Cryptography.X509Certificates.SubjectAlternativeNameBuilder]::new()
$san.AddDnsName('localhost')
$san.AddIpAddress([Net.IPAddress]::Loopback)
$request.CertificateExtensions.Add($san.Build())
$generated = $request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddDays(-1), [DateTimeOffset]::UtcNow.AddDays(1))
# Framework SslStream needs an imported private-key container for Schannel.
# No X509Store is opened; omitting PersistKeySet removes the temporary key on Dispose.
$certificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
    $generated.Export([Security.Cryptography.X509Certificates.X509ContentType]::Pfx, 'fixture'),
    'fixture', [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::UserKeySet)
$generated.Dispose()
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
try {
    $listener.Start()
    Set-Content -LiteralPath (Join-Path $WorkDirectory 'tls-port.txt') -Value $listener.LocalEndpoint.Port -Encoding ASCII
    for ($i = 0; $i -lt 2; ++$i) {
        $waiting = [Diagnostics.Stopwatch]::StartNew()
        while (!$listener.Pending()) {
            if ((Test-Path -LiteralPath (Join-Path $WorkDirectory 'stop')) -or $waiting.Elapsed.TotalSeconds -gt 10) { return }
            Start-Sleep -Milliseconds 50
        }
        $client = $listener.AcceptTcpClient()
        $client.ReceiveTimeout = 5000
        $client.SendTimeout = 5000
        $ssl = [Net.Security.SslStream]::new($client.GetStream(), $false)
        $ssl.ReadTimeout = 5000
        $ssl.WriteTimeout = 5000
        try {
            $ssl.AuthenticateAsServer($certificate, $false, [Security.Authentication.SslProtocols]::Tls12, $false)
            $buffer = New-Object byte[] 4096
            [void]$ssl.Read($buffer, 0, $buffer.Length)
            $response = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 200 OK`r`nContent-Length: 2`r`nConnection: close`r`n`r`nok")
            $ssl.Write($response, 0, $response.Length)
            $ssl.Flush()
            Write-Output 'Client accepted the untrusted certificate.'
        } catch {
            Write-Output ('TLS connection ended: ' + $_.Exception.ToString())
        } finally { $ssl.Dispose(); $client.Dispose() }
    }
} finally { $listener.Stop(); $certificate.Dispose(); $rsa.Dispose() }
