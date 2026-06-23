$Port = "COM3"
$Baud = 115200
$Minutes = 60
$OutFile = ""

if ($args.Length -ge 1) { $Minutes = [int]$args[0] }

if (-not $OutFile) {
    $OutFile = Join-Path (Join-Path $PSScriptRoot "..") "p3_log.txt"
}

try {
    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = $Port
    $port.BaudRate = $Baud
    $port.Open()
    Write-Host "P3 capture started on $Port at $(Get-Date -Format 'HH:mm:ss')"
    Write-Host "Running for $Minutes minutes. Logging to: $OutFile"
    Write-Host "Press Ctrl+C to stop early." -ForegroundColor Yellow

    $start = Get-Date
    $end = $start.AddMinutes($Minutes)
    while ((Get-Date) -lt $end) {
        if ($port.BytesToRead -gt 0) {
            try {
                $line = $port.ReadLine()
                $timestamp = Get-Date -Format "HH:mm:ss.fff"
                $logline = "[$timestamp] $line"
                $logline | Out-File -FilePath $OutFile -Append -Encoding utf8
                Write-Host $logline
            } catch {
                # read error, continue
            }
        } else {
            Start-Sleep -Milliseconds 300
        }
    }

    $elapsed = [math]::Round(((Get-Date) - $start).TotalMinutes, 1)
    Write-Host "P3 complete: ${elapsed}min logged to $OutFile" -ForegroundColor Green
}
catch {
    Write-Error "Failed: $_"
}
finally {
    if ($port -and $port.IsOpen) { $port.Close() }
    Write-Host "Port closed."
}
