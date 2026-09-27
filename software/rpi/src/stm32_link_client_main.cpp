#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <cstddef>
#include <chrono>
#include "linux_serial_port.h"
#include "stm32_link_session.h"
#include <optional>
#include <thread>

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

    std::random_device random_device;

    std::mt19937_64 random_generator(
        random_device());

    LinuxSerialPort serial_port;
    Stm32LinkSession session;

    while (true)
    {
        if (!serial_port.open(device_path))
        {
            std::cerr
                << "Serial device unavailable, retrying..."
                << std::endl;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));

            continue;
        }

        std::cout
            << "Opened serial device: "
            << device_path
            << std::endl;

        session.disconnect();

        ProtocolFrameDecoder decoder = {};

        protocol_frame_decoder_init(
            &decoder);

        const auto epoch_start =
            std::chrono::steady_clock::now();

        const std::uint64_t sync_token =
            random_generator();

        const ProtocolFrame sync_frame =
            session.begin_synchronization(
                sync_token);

        std::uint8_t sync_wire_data[
            PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

        const std::size_t sync_wire_size =
            protocol_frame_encode(
                &sync_frame,
                sync_wire_data);

        if (sync_wire_size == 0U)
        {
            std::cerr
                << "Failed to encode LINK_SYNC"
                << std::endl;

            return 1;
        }

        bool transport_failed = false;
        std::size_t sync_bytes_sent = 0U;

        while (sync_bytes_sent <
               sync_wire_size)
        {
            const LinuxSerialPollResult poll_result =
                serial_port.wait(
                    true,
                    50);

            if (poll_result.disconnected)
            {
                transport_failed = true;
                break;
            }

            if (!poll_result.writable)
            {
                continue;
            }

            const std::ptrdiff_t written =
                serial_port.write_some(
                    sync_wire_data +
                        sync_bytes_sent,
                    sync_wire_size -
                        sync_bytes_sent);

            if (written < 0)
            {
                transport_failed = true;
                break;
            }

            if (written == 0)
            {
                continue;
            }

            sync_bytes_sent +=
                static_cast<std::size_t>(
                    written);
        }

        if (transport_failed)
        {
            session.disconnect();
            serial_port.close();

            std::cerr
                << "STM32 link lost while sending LINK_SYNC"
                << std::endl;

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));

            continue;
        }

        std::cout
            << "LINK_SYNC sent: "
            << sync_bytes_sent
            << " bytes"
            << std::endl;

        const auto sync_deadline =
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(1000);

        bool synchronized = false;

        while (!synchronized &&
               !transport_failed)
        {
            if (std::chrono::steady_clock::now() >=
                sync_deadline)
            {
                break;
            }

            const LinuxSerialPollResult poll_result =
                serial_port.wait(
                    false,
                    50);

            if (poll_result.disconnected)
            {
                transport_failed = true;
                break;
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
                transport_failed = true;
                break;
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
                            current_time -
                            epoch_start)
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

        if (!synchronized)
        {
            session.disconnect();
            serial_port.close();

            if (transport_failed)
            {
                std::cerr
                    << "STM32 link lost during synchronization"
                    << std::endl;
            }
            else
            {
                std::cerr
                    << "LINK_SYNC timed out"
                    << std::endl;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(500));

            continue;
        }

        std::cout
            << "STM32 link synchronized"
            << std::endl;

        while (session.state() ==
               Stm32LinkState::Synchronized)
        {
            const auto current_time =
                std::chrono::steady_clock::now();

            const auto elapsed_ms =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                        current_time -
                        epoch_start)
                    .count();

            const auto now_ms =
                static_cast<std::uint32_t>(
                    elapsed_ms);

            session.check_link_timeout(
                now_ms);

            if (session.state() !=
                Stm32LinkState::Synchronized)
            {
                std::cerr
                    << "STM32 link timed out"
                    << std::endl;

                break;
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

                while (bytes_sent <
                       wire_size)
                {
                    const LinuxSerialPollResult poll_result =
                        serial_port.wait(
                            true,
                            50);

                    if (poll_result.disconnected)
                    {
                        transport_failed = true;
                        break;
                    }

                    if (!poll_result.writable)
                    {
                        continue;
                    }

                    const std::ptrdiff_t written =
                        serial_port.write_some(
                            wire_data +
                                bytes_sent,
                            wire_size -
                                bytes_sent);

                    if (written < 0)
                    {
                        transport_failed = true;
                        break;
                    }

                    if (written == 0)
                    {
                        continue;
                    }

                    bytes_sent +=
                        static_cast<std::size_t>(
                            written);
                }

                if (transport_failed)
                {
                    break;
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
                transport_failed = true;
                break;
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
                transport_failed = true;
                break;
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
                            response_time -
                            epoch_start)
                        .count();

                if (session.handle_heartbeat_response(
                        *frame,
                        static_cast<std::uint32_t>(
                            response_elapsed_ms)))
                {
                    std::cout
                        << "HEARTBEAT response accepted, sequence "
                        << frame->sequence
                        << std::endl;
                }
            }
        }

        session.disconnect();
        serial_port.close();

        if (transport_failed)
        {
            std::cerr
                << "STM32 link lost: serial transport disconnected"
                << std::endl;
        }

        std::cout
            << "Reconnecting in 500 ms..."
            << std::endl;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));
    }
}