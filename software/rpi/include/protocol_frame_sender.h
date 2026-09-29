#pragma once

#include <chrono>

#include "linux_serial_port.h"
#include "protocol_frame.h"

[[nodiscard]] bool send_protocol_frame(
    LinuxSerialPort& serial_port,
    const ProtocolFrame& frame,
    std::chrono::milliseconds timeout);