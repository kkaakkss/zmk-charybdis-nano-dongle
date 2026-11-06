param(
    [Parameter(Mandatory=$true)][ValidatePattern('^COM[0-9]+$')][string]$Port,
    [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$serial = [System.IO.Ports.SerialPort]::new($Port,115200)
$serial.DtrEnable = $true
$serial.ReadTimeout = 500
$writer = [System.IO.StreamWriter]::new($OutputPath,$true,[System.Text.UTF8Encoding]::new($false))
try {
    $serial.Open()
    Write-Host "Logging $Port to $OutputPath. Ctrl+C to stop."
    while ($true) {
        try {
            $line = $serial.ReadLine()
            $entry = "$(Get-Date -Format o) $line"
            $writer.WriteLine($entry)
            $writer.Flush()
            Write-Host $entry
        } catch [System.TimeoutException] { }
    }
} finally {
    $serial.Dispose()
    $writer.Dispose()
}
