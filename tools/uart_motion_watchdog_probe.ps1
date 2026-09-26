param(
    [string]$PortName = "COM3",
    [int]$BaudRate = 115200,
    [int]$ResponseTimeoutMs = 2000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"


# ---------------------------------------------------------------------------
# Protocol helpers
# ---------------------------------------------------------------------------

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
                $crc =
                (($crc -shl 1) -bxor 0x1021) -band 0xFFFF
            }
            else {
                $crc =
                ($crc -shl 1) -band 0xFFFF
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
    [byte[]]$frame =
    New-Object byte[] ($crcOffset + 2)

    $frame[0] = 0xA5
    $frame[1] = 0x5A
    $frame[2] = 0x01
    $frame[3] = $Type

    $frame[4] =
    ($Sequence -shr 8) -band 0xFF

    $frame[5] =
    $Sequence -band 0xFF

    $frame[6] =
    ($Payload.Length -shr 8) -band 0xFF

    $frame[7] =
    $Payload.Length -band 0xFF

    [Array]::Copy(
            $Payload,
            0,
            $frame,
            8,
            $Payload.Length)

    [byte[]]$crcData =
    $frame[2..($crcOffset - 1)]

    [uint16]$crc =
    Get-Crc16CcittFalse -Data $crcData

    $frame[$crcOffset] =
    ($crc -shr 8) -band 0xFF

    $frame[$crcOffset + 1] =
    $crc -band 0xFF

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

    [byte[]]$data =
    New-Object byte[] $Length

    [int]$offset = 0

    $timer =
    [System.Diagnostics.Stopwatch]::StartNew()

    while ($offset -lt $Length) {
        [int]$remainingMs =
        $TimeoutMs -
                [int]$timer.ElapsedMilliseconds

        if ($remainingMs -le 0) {
            throw "Timed out after $TimeoutMs ms while reading $Length bytes; received $offset."
        }

        $Port.ReadTimeout =
        [Math]::Max(1, $remainingMs)

        try {
            [int]$read =
            $Port.Read(
                    $data,
                    $offset,
                    $Length - $offset)

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

    $timer =
    [System.Diagnostics.Stopwatch]::StartNew()

    [byte[]]$header =
    Read-Exact `
            -Port $Port `
            -Length 8 `
            -TimeoutMs $TimeoutMs

    if (($header[0] -ne 0xA5) -or
            ($header[1] -ne 0x5A)) {
        throw "Invalid frame magic: $([BitConverter]::ToString($header[0..1]))."
    }

    if ($header[2] -ne 0x01) {
        throw (
        "Unsupported protocol version 0x{0:X2}." -f
        $header[2]
        )
    }

    [int]$payloadLength =
    ([int]$header[6] -shl 8) -bor
            [int]$header[7]

    if ($payloadLength -gt 64) {
        throw "Frame declares invalid payload length $payloadLength."
    }

    [int]$remainingMs =
    $TimeoutMs -
            [int]$timer.ElapsedMilliseconds

    if ($remainingMs -le 0) {
        throw "Timed out after reading the frame header."
    }

    [byte[]]$tail =
    Read-Exact `
            -Port $Port `
            -Length ($payloadLength + 2) `
            -TimeoutMs $remainingMs

    [byte[]]$frame =
    New-Object byte[] (8 + $tail.Length)

    [Array]::Copy(
            $header,
            0,
            $frame,
            0,
            8)

    [Array]::Copy(
            $tail,
            0,
            $frame,
            8,
            $tail.Length)

    [int]$crcOffset =
    8 + $payloadLength

    [byte[]]$crcData =
    $frame[2..($crcOffset - 1)]

    [uint16]$calculatedCrc =
    Get-Crc16CcittFalse -Data $crcData

    [uint16]$receivedCrc =
    ([uint16]$frame[$crcOffset] -shl 8) -bor
            [uint16]$frame[$crcOffset + 1]

    if ($receivedCrc -ne $calculatedCrc) {
        throw (
        "CRC mismatch: received 0x{0:X4}, calculated 0x{1:X4}." -f
        $receivedCrc,
        $calculatedCrc
        )
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

    Write-Host (
    "TX $Label`: {0}" -f
    [BitConverter]::ToString($Frame)
    )

    $Port.Write(
            $Frame,
            0,
            $Frame.Length)
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
        throw (
        "Wrong message type: 0x{0:X2}; expected 0x{1:X2}." -f
        $Frame[3],
        $ExpectedType
        )
    }

    if ($sequence -ne $ExpectedSequence) {
        throw (
        "Wrong sequence: $sequence; expected $ExpectedSequence."
        )
    }

    if ($payloadLength -ne $ExpectedPayloadLength) {
        throw (
        "Wrong payload length: $payloadLength; expected $ExpectedPayloadLength."
        )
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


function Get-Int16BigEndianBytes {
    param(
        [Parameter(Mandatory)]
        [int16]$Value
    )

    [int]$bits =
    ([int]$Value) -band 0xFFFF

    return [byte[]]@(
        (($bits -shr 8) -band 0xFF),
        ($bits -band 0xFF)
    )
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
        throw (
        "Wrong link state: 0x{0:X2}; expected 0x{1:X2}." -f
        $Frame[8],
        $ExpectedLinkState
        )
    }

    [uint32]$stm32UptimeMs =
    ConvertFrom-UInt32BigEndian `
            -Data $Frame `
            -Offset 9

    Write-Host (
    "STM32 uptime: $stm32UptimeMs ms"
    )
}


function Assert-MotionAck {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Frame,

        [Parameter(Mandatory)]
        [uint16]$ExpectedSequence
    )

    Assert-FrameContract `
        -Frame $Frame `
        -ExpectedType 0x11 `
        -ExpectedSequence $ExpectedSequence `
        -ExpectedPayloadLength 1

    if ($Frame[8] -ne 0x00) {
        throw (
        "Wrong MOTION_ACK status: 0x{0:X2}; expected ACCEPTED (0x00)." -f
        $Frame[8]
        )
    }
}


function Assert-MotionResponse {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Frame,

        [Parameter(Mandatory)]
        [uint16]$ExpectedSequence,

        [Parameter(Mandatory)]
        [byte]$ExpectedCommand,

        [Parameter(Mandatory)]
        [uint32]$ExpectedSessionId,

        [Parameter(Mandatory)]
        [byte]$ExpectedResult
    )

    Assert-FrameContract `
        -Frame $Frame `
        -ExpectedType 0x12 `
        -ExpectedSequence $ExpectedSequence `
        -ExpectedPayloadLength 6

    if ($Frame[8] -ne $ExpectedCommand) {
        throw (
        "Wrong MOTION_RESPONSE command: 0x{0:X2}; expected 0x{1:X2}." -f
        $Frame[8],
        $ExpectedCommand
        )
    }

    [uint32]$receivedSessionId =
    ConvertFrom-UInt32BigEndian `
            -Data $Frame `
            -Offset 9

    if ($receivedSessionId -ne $ExpectedSessionId) {
        throw (
        "Wrong motion session ID: $receivedSessionId; expected $ExpectedSessionId."
        )
    }

    if ($Frame[13] -ne $ExpectedResult) {
        throw (
        "Wrong MOTION_RESPONSE result: 0x{0:X2}; expected 0x{1:X2}." -f
        $Frame[13],
        $ExpectedResult
        )
    }
}


# ---------------------------------------------------------------------------
# Test configuration
# ---------------------------------------------------------------------------

[uint32]$motionSessionId = 1

# Requested physical velocity.
#
# With our temporary ARC101 feed-forward configuration:
#
#     100 mm/s -> motor command 850
#
# which should be above the observed startup threshold (~800).
[int16]$leftVelocityMmS = 100
[int16]$rightVelocityMmS = 100

# Keep heartbeats comfortably faster than the 1000 ms link timeout.
[int]$heartbeatIntervalMs = 100

# Keep sending heartbeats long enough to observe the 250 ms
# motion watchdog expire while the link remains alive.
[int]$heartbeatCount = 7


# ---------------------------------------------------------------------------
# Sequence numbers
# ---------------------------------------------------------------------------

[uint16]$linkSyncSequence = 1
[uint16]$motionStartSequence = 2
[uint16]$wheelVelocitySequence = 3

[uint16]$nextHeartbeatSequence = 4


# ---------------------------------------------------------------------------
# Payloads
# ---------------------------------------------------------------------------

[byte[]]$linkSyncToken = @(
    0x01, 0x23, 0x45, 0x67,
    0x89, 0xAB, 0xCD, 0xEF
)


# START_SESSION = 0x01
# motion_session_id = 1
[byte[]]$motionStartPayload = @(
    0x01,
    0x00, 0x00, 0x00, 0x01
)


# WHEEL_VELOCITY payload:
#
# uint32_t motion_session_id
# int16_t  left_velocity_mm_s
# int16_t  right_velocity_mm_s
#
[byte[]]$leftVelocityBytes =
Get-Int16BigEndianBytes `
        -Value $leftVelocityMmS

[byte[]]$rightVelocityBytes =
Get-Int16BigEndianBytes `
        -Value $rightVelocityMmS

[byte[]]$wheelVelocityPayload = @(
    0x00, 0x00, 0x00, 0x01,
    $leftVelocityBytes[0],
    $leftVelocityBytes[1],
    $rightVelocityBytes[0],
    $rightVelocityBytes[1]
)


# Heartbeat payload:
#
# link_state = SYNCHRONIZED (0x01)
# peer uptime field = 1000 ms
#
[byte[]]$synchronizedHeartbeatPayload = @(
    0x01,
    0x00, 0x00, 0x03, 0xE8
)


# ---------------------------------------------------------------------------
# Frames
# ---------------------------------------------------------------------------

[byte[]]$linkSyncRequest =
New-ProtocolFrame `
        -Type 0x01 `
        -Sequence $linkSyncSequence `
        -Payload $linkSyncToken


[byte[]]$motionStartRequest =
New-ProtocolFrame `
        -Type 0x10 `
        -Sequence $motionStartSequence `
        -Payload $motionStartPayload


[byte[]]$wheelVelocityRequest =
New-ProtocolFrame `
        -Type 0x20 `
        -Sequence $wheelVelocitySequence `
        -Payload $wheelVelocityPayload


# ---------------------------------------------------------------------------
# Serial port
# ---------------------------------------------------------------------------

$port =
[System.IO.Ports.SerialPort]::new(
        $PortName,
        $BaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
)

$port.WriteTimeout =
$ResponseTimeoutMs


try {
    $port.Open()

    $port.DiscardInBuffer()
    $port.DiscardOutBuffer()


    # -----------------------------------------------------------------------
    # 1. Synchronize link
    # -----------------------------------------------------------------------

    Write-ProtocolFrame `
        -Port $port `
        -Frame $linkSyncRequest `
        -Label "LINK_SYNC"

    [byte[]]$linkSyncResponse =
    Read-ProtocolFrame `
            -Port $port `
            -TimeoutMs $ResponseTimeoutMs

    Write-Host (
    "RX LINK_SYNC_OK: {0}" -f
    [BitConverter]::ToString($linkSyncResponse)
    )

    Assert-FrameContract `
        -Frame $linkSyncResponse `
        -ExpectedType 0x02 `
        -ExpectedSequence $linkSyncSequence `
        -ExpectedPayloadLength 8

    [byte[]]$receivedToken =
    $linkSyncResponse[8..15]

    if ([BitConverter]::ToString($receivedToken) -ne
            [BitConverter]::ToString($linkSyncToken)) {
        throw "LINK_SYNC_OK did not echo the synchronization token."
    }


    # -----------------------------------------------------------------------
    # 2. Start motion session
    # -----------------------------------------------------------------------

    Write-ProtocolFrame `
        -Port $port `
        -Frame $motionStartRequest `
        -Label "MOTION_START session=$motionSessionId"

    [byte[]]$motionAck =
    Read-ProtocolFrame `
            -Port $port `
            -TimeoutMs $ResponseTimeoutMs

    Write-Host (
    "RX MOTION_ACK: {0}" -f
    [BitConverter]::ToString($motionAck)
    )

    Assert-MotionAck `
        -Frame $motionAck `
        -ExpectedSequence $motionStartSequence


    [byte[]]$motionResponse =
    Read-ProtocolFrame `
            -Port $port `
            -TimeoutMs $ResponseTimeoutMs

    Write-Host (
    "RX MOTION_RESPONSE: {0}" -f
    [BitConverter]::ToString($motionResponse)
    )

    Assert-MotionResponse `
        -Frame $motionResponse `
        -ExpectedSequence $motionStartSequence `
        -ExpectedCommand 0x01 `
        -ExpectedSessionId $motionSessionId `
        -ExpectedResult 0x00


    # -----------------------------------------------------------------------
    # 3. Send ONE wheel command
    # -----------------------------------------------------------------------

    Write-Host ""
    Write-Host "IMPORTANT: Watch the wheels now."
    Write-Host (
    "Expected: wheels start, then stop after about 250 ms."
    )
    Write-Host (
    "The script will continue sending HEARTBEAT only."
    )
    Write-Host ""

    $motionTimer =
    [System.Diagnostics.Stopwatch]::StartNew()

    Write-ProtocolFrame `
        -Port $port `
        -Frame $wheelVelocityRequest `
        -Label (
    "WHEEL_VELOCITY session=$motionSessionId " +
            "left=$leftVelocityMmS mm/s " +
            "right=$rightVelocityMmS mm/s"
    )

    Write-Host (
    "No more WHEEL_VELOCITY frames will be sent."
    )


    # -----------------------------------------------------------------------
    # 4. Keep LINK alive with heartbeat only
    #
    # This is the key test:
    #
    # heartbeat must refresh ONLY the 1000 ms link timeout.
    # It must NOT refresh the 250 ms motion watchdog.
    # -----------------------------------------------------------------------

    for ($index = 0;
         $index -lt $heartbeatCount;
         ++$index) {

        Start-Sleep `
            -Milliseconds $heartbeatIntervalMs

        [uint16]$heartbeatSequence =
        $nextHeartbeatSequence

        ++$nextHeartbeatSequence

        [byte[]]$heartbeatRequest =
        New-ProtocolFrame `
                -Type 0x03 `
                -Sequence $heartbeatSequence `
                -Payload $synchronizedHeartbeatPayload

        [long]$elapsedBeforeTx =
        $motionTimer.ElapsedMilliseconds

        Write-ProtocolFrame `
            -Port $port `
            -Frame $heartbeatRequest `
            -Label (
        "HEARTBEAT only " +
                "(~$elapsedBeforeTx ms after wheel command)"
        )

        [byte[]]$heartbeatResponse =
        Read-ProtocolFrame `
                -Port $port `
                -TimeoutMs $ResponseTimeoutMs

        Write-Host (
        "RX HEARTBEAT: {0}" -f
        [BitConverter]::ToString(
                $heartbeatResponse)
        )

        Assert-HeartbeatResponse `
            -Frame $heartbeatResponse `
            -ExpectedSequence $heartbeatSequence `
            -ExpectedLinkState 0x01

        Write-Host (
        "Link still SYNCHRONIZED at ~{0} ms" -f
        $motionTimer.ElapsedMilliseconds
        )
    }


    # -----------------------------------------------------------------------
    # 5. End the motion session cleanly
    # -----------------------------------------------------------------------

    [uint16]$motionEndSequence =
    $nextHeartbeatSequence

    [byte[]]$motionEndPayload = @(
        0x02,
        0x00, 0x00, 0x00, 0x01
    )

    [byte[]]$motionEndRequest =
    New-ProtocolFrame `
            -Type 0x10 `
            -Sequence $motionEndSequence `
            -Payload $motionEndPayload

    Write-ProtocolFrame `
        -Port $port `
        -Frame $motionEndRequest `
        -Label "MOTION_END session=$motionSessionId"

    [byte[]]$motionEndAck =
    Read-ProtocolFrame `
            -Port $port `
            -TimeoutMs $ResponseTimeoutMs

    Write-Host (
    "RX MOTION_ACK: {0}" -f
    [BitConverter]::ToString($motionEndAck)
    )

    Assert-MotionAck `
        -Frame $motionEndAck `
        -ExpectedSequence $motionEndSequence


    [byte[]]$motionEndResponse =
    Read-ProtocolFrame `
            -Port $port `
            -TimeoutMs $ResponseTimeoutMs

    Write-Host (
    "RX MOTION_RESPONSE: {0}" -f
    [BitConverter]::ToString(
            $motionEndResponse)
    )

    Assert-MotionResponse `
        -Frame $motionEndResponse `
        -ExpectedSequence $motionEndSequence `
        -ExpectedCommand 0x02 `
        -ExpectedSessionId $motionSessionId `
        -ExpectedResult 0x00


    Write-Host ""
    Write-Host "PASS: protocol side of watchdog test succeeded."
    Write-Host (
    "Heartbeats kept the UART link SYNCHRONIZED for " +
            "$($motionTimer.ElapsedMilliseconds) ms after the single wheel command."
    )

    Write-Host ""
    Write-Host "VISUAL CHECK:"
    Write-Host (
    "The wheels should have started after WHEEL_VELOCITY " +
            "and stopped approximately 250 ms later, while the " +
            "HEARTBEAT responses continued to report SYNCHRONIZED."
    )

    Write-Host ""
    Write-Host (
    "If that happened, the independent heartbeat / motion-watchdog " +
            "safety contract is hardware-validated."
    )
}
finally {
    if ($port.IsOpen) {
        $port.Close()
    }

    $port.Dispose()
}