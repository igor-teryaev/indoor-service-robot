#include "protocol_frame_sender.h"

#include <cstdint>

extern "C"
{
#include "protocol_frame_encoder.h"
}

bool send_protocol_frame(
    LinuxSerialPort& serial_port,
    const ProtocolFrame& frame,
    const std::chrono::milliseconds timeout)
{
    std::uint8_t wire_data[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t wire_size =
        protocol_frame_encode(
            &frame,
            wire_data);

    if (wire_size == 0U)
    {
        return false;
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

    std::size_t bytes_sent = 0U;

    while (bytes_sent < wire_size)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= deadline)
        {
            return false;
        }

        const auto remaining =
            std::chrono::ceil<std::chrono::milliseconds>(
                deadline - now);

        const LinuxSerialPollResult poll_result =
            serial_port.wait(
                true,
                static_cast<int>(
                    remaining.count()));

        if (poll_result.disconnected)
        {
            return false;
        }

        if (!poll_result.writable)
        {
            return false;
        }

        const std::ptrdiff_t written =
            serial_port.write_some(
                wire_data + bytes_sent,
                wire_size - bytes_sent);

        if (written < 0)
        {
            return false;
        }

        if (written == 0)
        {
            continue;
        }

        bytes_sent +=
            static_cast<std::size_t>(
                written);
    }

    return true;
}