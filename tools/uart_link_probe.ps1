param(
    [string]$PortName = "COM3",
    [int]$BaudRate = 115200,
    [int]$ResponseTimeoutMs = 2000,
    [int]$LinkTimeoutWaitMs = 1200
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-Crc16CcittFalse {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Data
    )

    [int]$crc = 0xFFFF

    foreach ($value in $Data) {
        $crc = $crc -bxor ([int]$value -shl 8)

        for ($bit = 0; $bit -lt 8; ++$bit) {
            if (($crc -band 0x8000) -ne 0) {
                $crc = (($crc -shl 1) -bxor 0x1021) -band 0xFFFF
            }
            else {
                $crc = ($crc -shl 1) -band 0xFFFF
            }
        }
    }

    return [uint16]$crc
}

function New-ProtocolFrame {
    param(
        [Parameter(Mandatory)]
        [byte]$Type,

        [Parameter(Mandatory)]
        [uint16]$Sequence,

        [Parameter(Mandatory)]
        [byte[]]$Payload
    )

    if ($Payload.Length -gt 64) {
        throw "Payload length $($Payload.Length) exceeds the 64-byte protocol limit."
    }

    [int]$crcOffset = 8 + $Payload.Length
    [byte[]]$frame = New-Object byte[] ($crcOffset + 2)

    $frame[0] = 0xA5
    $frame[1] = 0x5A
    $frame[2] = 0x01
    $frame[3] = $Type
    $frame[4] = ($Sequence -shr 8) -band 0xFF
    $frame[5] = $Sequence -band 0xFF
    $frame[6] = ($Payload.Length -shr 8) -band 0xFF
    $frame[7] = $Payload.Length -band 0xFF

    [Array]::Copy($Payload, 0, $frame, 8, $Payload.Length)

    [byte[]]$crcData = $frame[2..($crcOffset - 1)]
    [uint16]$crc = Get-Crc16CcittFalse -Data $crcData
    $frame[$crcOffset] = ($crc -shr 8) -band 0xFF
    $frame[$crcOffset + 1] = $crc -band 0xFF

    return ,$frame
}

function Read-Exact {
    param(
        [Parameter(Mandatory)]
        [System.IO.Ports.SerialPort]$Port,

        [Parameter(Mandatory)]
        [int]$Length,

        [Parameter(Mandatory)]
        [int]$TimeoutMs
    )

    [byte[]]$data = New-Object byte[] $Length
    [int]$offset = 0
    $timer = [System.Diagnostics.Stopwatch]::StartNew()

    while ($offset -lt $Length) {
        [int]$remainingMs = $TimeoutMs - [int]$timer.ElapsedMilliseconds
        if ($remainingMs -le 0) {
            throw "Timed out after $TimeoutMs ms while reading $Length bytes; received $offset."
        }

        $Port.ReadTimeout = [Math]::Max(1, $remainingMs)

        try {
            [int]$read = $Port.Read($data, $offset, $Length - $offset)
            $offset += $read
        }
        catch [System.TimeoutException] {
            throw "Timed out after $TimeoutMs ms while reading $Length bytes; received $offset."
        }
    }

    return ,$data
}

function Read-ProtocolFrame {
    param(
        [Parameter(Mandatory)]
        [System.IO.Ports.SerialPort]$Port,

        [Parameter(Mandatory)]
        [int]$TimeoutMs
    )

    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    [byte[]]$header = Read-Exact -Port $Port -Length 8 -TimeoutMs $TimeoutMs

    if (($header[0] -ne 0xA5) -or ($header[1] -ne 0x5A)) {
        throw "Invalid frame magic: $([BitConverter]::ToString($header[0..1]))."
    }

    if ($header[2] -ne 0x01) {
        throw ("Unsupported protocol version 0x{0:X2}." -f $header[2])
    }

    [int]$payloadLength = ([int]$header[6] -shl 8) -bor [int]$header[7]
    if ($payloadLength -gt 64) {
        throw "Frame declares invalid payload length $payloadLength."
    }

    [int]$remainingMs = $TimeoutMs - [int]$timer.ElapsedMilliseconds
    if ($remainingMs -le 0) {
        throw "Timed out after reading the frame header."
    }

    [byte[]]$tail = Read-Exact `
        -Port $Port `
        -Length ($payloadLength + 2) `
        -TimeoutMs $remainingMs

    [byte[]]$frame = New-Object byte[] (8 + $tail.Length)
    [Array]::Copy($header, 0, $frame, 0, 8)
    [Array]::Copy($tail, 0, $frame, 8, $tail.Length)

    [int]$crcOffset = 8 + $payloadLength
    [byte[]]$crcData = $frame[2..($crcOffset - 1)]
    [uint16]$calculatedCrc = Get-Crc16CcittFalse -Data $crcData
    [uint16]$receivedCrc =
        ([uint16]$frame[$crcOffset] -shl 8) -bor
        [uint16]$frame[$crcOffset + 1]

    if ($receivedCrc -ne $calculatedCrc) {
        throw ("CRC mismatch: received 0x{0:X4}, calculated 0x{1:X4}." -f
            $receivedCrc, $calculatedCrc)
    }

    return ,$frame
}

function Write-ProtocolFrame {
    param(
        [Parameter(Mandatory)]
        [System.IO.Ports.SerialPort]$Port,

        [Parameter(Mandatory)]
        [byte[]]$Frame,

        [Parameter(Mandatory)]
        [string]$Label
    )

    Write-Host "TX $Label`: $([BitConverter]::ToString($Frame))"
    $Port.Write($Frame, 0, $Frame.Length)
}

function Assert-FrameContract {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Frame,

        [Parameter(Mandatory)]
        [byte]$ExpectedType,

        [Parameter(Mandatory)]
        [uint16]$ExpectedSequence,

        [Parameter(Mandatory)]
        [int]$ExpectedPayloadLength
    )

    [uint16]$sequence =
        ([uint16]$Frame[4] -shl 8) -bor
        [uint16]$Frame[5]

    [int]$payloadLength =
        ([int]$Frame[6] -shl 8) -bor
        [int]$Frame[7]

    if ($Frame[3] -ne $ExpectedType) {
        throw ("Wrong message type: 0x{0:X2}; expected 0x{1:X2}." -f
            $Frame[3], $ExpectedType)
    }

    if ($sequence -ne $ExpectedSequence) {
        throw "Wrong sequence: $sequence; expected $ExpectedSequence."
    }

    if ($payloadLength -ne $ExpectedPayloadLength) {
        throw "Wrong payload length: $payloadLength; expected $ExpectedPayloadLength."
    }
}

function ConvertFrom-UInt32BigEndian {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Data,

        [Parameter(Mandatory)]
        [int]$Offset
    )

    [uint64]$value =
        ([uint64]$Data[$Offset] * 16777216) +
        ([uint64]$Data[$Offset + 1] * 65536) +
        ([uint64]$Data[$Offset + 2] * 256) +
        [uint64]$Data[$Offset + 3]

    return [uint32]$value
}

function Assert-HeartbeatResponse {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Frame,

        [Parameter(Mandatory)]
        [uint16]$ExpectedSequence,

        [Parameter(Mandatory)]
        [byte]$ExpectedLinkState
    )

    Assert-FrameContract `
        -Frame $Frame `
        -ExpectedType 0x03 `
        -ExpectedSequence $ExpectedSequence `
        -ExpectedPayloadLength 5

    if ($Frame[8] -ne $ExpectedLinkState) {
        throw ("Wrong link state: 0x{0:X2}; expected 0x{1:X2}." -f
            $Frame[8], $ExpectedLinkState)
    }

    [uint32]$stm32UptimeMs = ConvertFrom-UInt32BigEndian -Data $Frame -Offset 9
    Write-Host "STM32 uptime: $stm32UptimeMs ms"
}

[uint16]$linkSyncSequence = 1
[uint16]$heartbeatSequence = 2

[byte[]]$linkSyncToken = @(
    0x01, 0x23, 0x45, 0x67,
    0x89, 0xAB, 0xCD, 0xEF
)

[byte[]]$synchronizedHeartbeatPayload = @(
    0x01,
    0x00, 0x00, 0x03, 0xE8
)

[byte[]]$linkSyncRequest = New-ProtocolFrame `
    -Type 0x01 `
    -Sequence $linkSyncSequence `
    -Payload $linkSyncToken

[byte[]]$heartbeatRequest = New-ProtocolFrame `
    -Type 0x03 `
    -Sequence $heartbeatSequence `
    -Payload $synchronizedHeartbeatPayload

$port = [System.IO.Ports.SerialPort]::new(
    $PortName,
    $BaudRate,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)

$port.WriteTimeout = $ResponseTimeoutMs

try {
    $port.Open()
    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()

    Write-ProtocolFrame -Port $port -Frame $linkSyncRequest -Label "LINK_SYNC"
    [byte[]]$linkSyncResponse = Read-ProtocolFrame `
        -Port $port `
        -TimeoutMs $ResponseTimeoutMs

    Write-Host "RX LINK_SYNC_OK: $([BitConverter]::ToString($linkSyncResponse))"
    Assert-FrameContract `
        -Frame $linkSyncResponse `
        -ExpectedType 0x02 `
        -ExpectedSequence $linkSyncSequence `
        -ExpectedPayloadLength 8

    [byte[]]$receivedToken = $linkSyncResponse[8..15]
    if ([BitConverter]::ToString($receivedToken) -ne
        [BitConverter]::ToString($linkSyncToken)) {
        throw "LINK_SYNC_OK did not echo the synchronization token."
    }

    Start-Sleep -Milliseconds 100

    Write-ProtocolFrame -Port $port -Frame $heartbeatRequest -Label "HEARTBEAT synchronized"
    [byte[]]$synchronizedResponse = Read-ProtocolFrame `
        -Port $port `
        -TimeoutMs $ResponseTimeoutMs

    Write-Host "RX HEARTBEAT: $([BitConverter]::ToString($synchronizedResponse))"
    Assert-HeartbeatResponse `
        -Frame $synchronizedResponse `
        -ExpectedSequence $heartbeatSequence `
        -ExpectedLinkState 0x01

    Write-Host "Waiting $LinkTimeoutWaitMs ms without heartbeat..."
    Start-Sleep -Milliseconds $LinkTimeoutWaitMs

    Write-ProtocolFrame -Port $port -Frame $heartbeatRequest -Label "HEARTBEAT after timeout"
    [byte[]]$unsynchronizedResponse = Read-ProtocolFrame `
        -Port $port `
        -TimeoutMs $ResponseTimeoutMs

    Write-Host "RX HEARTBEAT: $([BitConverter]::ToString($unsynchronizedResponse))"
    Assert-HeartbeatResponse `
        -Frame $unsynchronizedResponse `
        -ExpectedSequence $heartbeatSequence `
        -ExpectedLinkState 0x00

    Write-Host "PASS: LINK_SYNC succeeded, heartbeat preserved the link, and timeout cleared it."
}
finally {
    if ($port.IsOpen) {
        $port.Close()
    }

    $port.Dispose()
}
