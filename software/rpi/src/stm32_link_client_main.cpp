#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <cstddef>
#include <chrono>
#include "linux_serial_port.h"
#include "stm32_link_session.h"
#include <optional>

extern "C"
{
#include "protocol_frame_encoder.h"
#include "protocol_frame_decoder.h"
}

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: stm32_link_client <serial-device>"
            << std::endl;

        return 1;
    }

    const std::string device_path =
        argv[1];

    LinuxSerialPort serial_port;

    if (!serial_port.open(device_path))
    {
        std::cerr
            << "Failed to open serial device: "
            << device_path
            << std::endl;

        return 1;
    }

    std::cout
        << "Opened serial device: "
        << device_path
        << std::endl;

    std::random_device random_device;
    std::mt19937_64 random_generator(
        random_device());

    const std::uint64_t sync_token =
        random_generator();

    Stm32LinkSession session;

    const ProtocolFrame sync_frame =
        session.begin_synchronization(
            sync_token);

    std::uint8_t wire_data[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t wire_size =
        protocol_frame_encode(
            &sync_frame,
            wire_data);

    if (wire_size == 0U)
    {
        std::cerr
            << "Failed to encode LINK_SYNC frame"
            << std::endl;

        return 1;
    }

    std::size_t bytes_sent = 0U;

    while (bytes_sent < wire_size)
    {
        const LinuxSerialPollResult poll_result =
            serial_port.wait(
                true,
                50);

        if (poll_result.disconnected)
        {
            std::cerr
                << "Serial device disconnected while sending LINK_SYNC"
                << std::endl;

            return 1;
        }

        if (!poll_result.writable)
        {
            continue;
        }

        const std::ptrdiff_t written =
            serial_port.write_some(
                wire_data + bytes_sent,
                wire_size - bytes_sent);

        if (written < 0)
        {
            std::cerr
                << "Failed to write LINK_SYNC"
                << std::endl;

            return 1;
        }

        if (written == 0)
        {
            continue;
        }

        bytes_sent +=
            static_cast<std::size_t>(
                written);
    }

    std::cout
        << "LINK_SYNC sent: "
        << bytes_sent
        << " bytes"
        << std::endl;


    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    const auto start_time =
        std::chrono::steady_clock::now();

    const auto sync_deadline =
        start_time +
        std::chrono::milliseconds(1000);

    bool synchronized = false;

    while (!synchronized)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= sync_deadline)
        {
            std::cerr
                << "Timed out waiting for LINK_SYNC_OK"
                << std::endl;

            return 1;
        }

        const LinuxSerialPollResult poll_result =
            serial_port.wait(
                false,
                50);

        if (poll_result.disconnected)
        {
            std::cerr
                << "Serial device disconnected while waiting for LINK_SYNC_OK"
                << std::endl;

            return 1;
        }

        if (!poll_result.readable)
        {
            continue;
        }

        std::uint8_t read_buffer[128] = {};

        const std::ptrdiff_t bytes_read =
            serial_port.read_some(
                read_buffer,
                sizeof(read_buffer));

        if (bytes_read < 0)
        {
            std::cerr
                << "Failed to read serial device"
                << std::endl;

            return 1;
        }

        for (std::ptrdiff_t i = 0;
             i < bytes_read;
             ++i)
        {
            const ProtocolFrame* frame =
                nullptr;

            if (!protocol_frame_decoder_feed_byte(
                    &decoder,
                    read_buffer[i],
                    &frame))
            {
                continue;
            }

            const auto current_time =
                std::chrono::steady_clock::now();

            const auto elapsed_ms =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                        current_time - start_time)
                    .count();

            if (session.handle_link_sync_ok(
                    *frame,
                    static_cast<std::uint32_t>(
                        elapsed_ms)))
            {
                synchronized = true;
                break;
            }
        }
    }

    std::cout
        << "STM32 link synchronized"
        << std::endl;

    const auto soak_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::seconds(5);

std::size_t accepted_heartbeats = 0U;

while (std::chrono::steady_clock::now() <
       soak_deadline)
{
    const auto current_time =
        std::chrono::steady_clock::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                current_time - start_time)
            .count();

    const auto now_ms =
        static_cast<std::uint32_t>(
            elapsed_ms);

    session.check_link_timeout(now_ms);

    if (session.state() !=
        Stm32LinkState::Synchronized)
    {
        std::cerr
            << "STM32 link timed out"
            << std::endl;

        return 1;
    }

    const auto heartbeat_frame =
        session.heartbeat_if_due(
            now_ms);

    if (heartbeat_frame.has_value())
    {
        std::uint8_t wire_data[
            PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

        const std::size_t wire_size =
            protocol_frame_encode(
                &heartbeat_frame.value(),
                wire_data);

        if (wire_size == 0U)
        {
            std::cerr
                << "Failed to encode HEARTBEAT"
                << std::endl;

            return 1;
        }

        std::size_t bytes_sent = 0U;

        while (bytes_sent < wire_size)
        {
            const LinuxSerialPollResult poll_result =
                serial_port.wait(
                    true,
                    50);

            if (poll_result.disconnected)
            {
                std::cerr
                    << "Serial device disconnected"
                    << std::endl;

                return 1;
            }

            if (!poll_result.writable)
            {
                continue;
            }

            const std::ptrdiff_t written =
                serial_port.write_some(
                    wire_data + bytes_sent,
                    wire_size - bytes_sent);

            if (written < 0)
            {
                std::cerr
                    << "Failed to write HEARTBEAT"
                    << std::endl;

                return 1;
            }

            bytes_sent +=
                static_cast<std::size_t>(
                    written);
        }

        std::cout
            << "HEARTBEAT sent, sequence "
            << heartbeat_frame->sequence
            << std::endl;
    }

    const LinuxSerialPollResult poll_result =
        serial_port.wait(
            false,
            50);

    if (poll_result.disconnected)
    {
        std::cerr
            << "Serial device disconnected"
            << std::endl;

        return 1;
    }

    if (!poll_result.readable)
    {
        continue;
    }

    std::uint8_t read_buffer[128] = {};

    const std::ptrdiff_t bytes_read =
        serial_port.read_some(
            read_buffer,
            sizeof(read_buffer));

    if (bytes_read < 0)
    {
        std::cerr
            << "Failed to read serial device"
            << std::endl;

        return 1;
    }

    for (std::ptrdiff_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (!protocol_frame_decoder_feed_byte(
                &decoder,
                read_buffer[i],
                &frame))
        {
            continue;
        }

        const auto response_time =
            std::chrono::steady_clock::now();

        const auto response_elapsed_ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    response_time - start_time)
                .count();

        if (session.handle_heartbeat_response(
                *frame,
                static_cast<std::uint32_t>(
                    response_elapsed_ms)))
        {
            ++accepted_heartbeats;

            std::cout
                << "HEARTBEAT response accepted, sequence "
                << frame->sequence
                << std::endl;
        }
    }
}

std::cout
    << "5-second heartbeat soak complete: "
    << accepted_heartbeats
    << " responses accepted"
    << std::endl;

    return 0;
}