[CmdletBinding()]
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot ("vita2hos-{0:yyyyMMdd-HHmmss}.log" -f [DateTime]::Now)),
    [string]$InterfaceAddress,
    [string]$SwitchAddress,
    [ValidateRange(1, 65535)][int]$Port = 28771,
    [ValidateRange(0, 86400)][int]$DurationSeconds = 0
)

$ErrorActionPreference = 'Stop'
$client = $null
$stream = $null
try {
    $OutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
    $client = [Net.Sockets.UdpClient]::new([Net.Sockets.AddressFamily]::InterNetwork)
    $client.ExclusiveAddressUse = $false
    $client.Client.SetSocketOption([Net.Sockets.SocketOptionLevel]::Socket,
        [Net.Sockets.SocketOptionName]::ReuseAddress, $true)
    $client.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Any, $Port))
    $client.Client.ReceiveTimeout = 1000
    $group = [Net.IPAddress]::Parse('224.0.0.1')
    if ($InterfaceAddress) {
        $localAddress = [Net.IPAddress]::Parse($InterfaceAddress)
        if ($localAddress.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork) {
            throw 'InterfaceAddress must be an IPv4 address on this PC.'
        }
        $client.JoinMulticastGroup($group, $localAddress)
    } else {
        $client.JoinMulticastGroup($group)
    }
    if ($SwitchAddress) {
        $SwitchAddress = [Net.IPAddress]::Parse($SwitchAddress).ToString()
    }

    $stream = [IO.File]::Open($OutputPath, [IO.FileMode]::Append,
        [IO.FileAccess]::Write, [IO.FileShare]::Read)
    Write-Host "Listening on 224.0.0.1:$Port; saving to $OutputPath"
    Write-Host 'Start vita2hos on the same LAN. Press Ctrl+C to stop.'
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $remote = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
    while ($DurationSeconds -eq 0 -or $timer.Elapsed.TotalSeconds -lt $DurationSeconds) {
        try {
            $data = $client.Receive([ref]$remote)
        } catch [Net.Sockets.SocketException] {
            if ($_.Exception.SocketErrorCode -eq [Net.Sockets.SocketError]::TimedOut) {
                continue
            }
            throw
        }
        if ($SwitchAddress -and $remote.Address.ToString() -ne $SwitchAddress) {
            continue
        }
        $stream.Write($data, 0, $data.Length)
        $stream.Flush()
        [Console]::Write([Text.Encoding]::UTF8.GetString($data))
    }
} finally {
    if ($stream) { $stream.Dispose() }
    if ($client) { $client.Dispose() }
}
