#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include "linux_serial_port.h"
#include "stm32_motion_session.h"
#include <algorithm>
#include <poll.h>
#include <chrono>
#include "protocol_frame_sender.h"
#include "stm32_client_runner.h"

extern "C"
{
#include "link_sync_codec.h"
#include "protocol_frame_decoder.h"
#include "protocol_frame_encoder.h"
#include "protocol_message_type.h"
#include "motion_lifecycle_command_codec.h"
#include "motion_ack_codec.h"
#include "motion_response_codec.h"
#include "wheel_velocity_payload_codec.h"
#include "heartbeat_codec.h"
}

static ProtocolFrame read_frame_from_pty_master(
    const int master_fd)
{
    std::uint8_t buffer[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const ssize_t bytes_read =
        ::read(
            master_fd,
            buffer,
            sizeof(buffer));

    EXPECT_GT(bytes_read, 0);

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    ProtocolFrame decoded = {};
    bool complete = false;

    for (ssize_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (protocol_frame_decoder_feed_byte(
                &decoder,
                buffer[i],
                &frame))
        {
            decoded = *frame;
            complete = true;
            break;
        }
    }

    EXPECT_TRUE(complete);

    return decoded;
}

TEST(
    LinuxSerialPortPtyTest,
    OpensPtySlave)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    EXPECT_TRUE(
        serial_port.open(
            slave_name));

    EXPECT_TRUE(
        serial_port.is_open());

    serial_port.close();

    EXPECT_FALSE(
        serial_port.is_open());

    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    ReadsBytesFromPtyMaster)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    const std::uint8_t transmitted[] =
    {
        0xA5U,
        0x5AU,
        0x01U,
        0x02U
    };

    ASSERT_EQ(
        ::write(
            master_fd,
            transmitted,
            sizeof(transmitted)),
        static_cast<ssize_t>(
            sizeof(transmitted)));

    const LinuxSerialPollResult poll_result =
        serial_port.wait(
            false,
            100);

    ASSERT_TRUE(
        poll_result.readable);

    std::uint8_t received[16] = {};

    const std::ptrdiff_t bytes_read =
        serial_port.read_some(
            received,
            sizeof(received));

    ASSERT_EQ(
        bytes_read,
        static_cast<std::ptrdiff_t>(
            sizeof(transmitted)));

    for (std::size_t i = 0U;
         i < sizeof(transmitted);
         ++i)
    {
        EXPECT_EQ(
            received[i],
            transmitted[i]);
    }

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    WritesBytesToPtyMaster)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    const std::uint8_t transmitted[] =
    {
        0xA5U,
        0x5AU,
        0x10U,
        0x20U
    };

    const LinuxSerialPollResult poll_result =
        serial_port.wait(
            true,
            100);

    ASSERT_TRUE(
        poll_result.writable);

    const std::ptrdiff_t bytes_written =
        serial_port.write_some(
            transmitted,
            sizeof(transmitted));

    ASSERT_EQ(
        bytes_written,
        static_cast<std::ptrdiff_t>(
            sizeof(transmitted)));

    std::uint8_t received[16] = {};

    const ssize_t bytes_read =
        ::read(
            master_fd,
            received,
            sizeof(received));

    ASSERT_EQ(
        bytes_read,
        static_cast<ssize_t>(
            sizeof(transmitted)));

    for (std::size_t i = 0U;
         i < sizeof(transmitted);
         ++i)
    {
        EXPECT_EQ(
            received[i],
            transmitted[i]);
    }

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    DecodesFragmentedFrameFromPtyMaster)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame source_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 42U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        source_frame.payload);

    std::uint8_t wire_data[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t wire_size =
        protocol_frame_encode(
            &source_frame,
            wire_data);

    ASSERT_GT(
        wire_size,
        4U);

    const std::size_t first_part_size =
        4U;

    ASSERT_EQ(
        ::write(
            master_fd,
            wire_data,
            first_part_size),
        static_cast<ssize_t>(
            first_part_size));

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    const LinuxSerialPollResult first_poll =
        serial_port.wait(
            false,
            100);

    ASSERT_TRUE(
        first_poll.readable);

    std::uint8_t read_buffer[128] = {};

    const std::ptrdiff_t first_read =
        serial_port.read_some(
            read_buffer,
            sizeof(read_buffer));

    ASSERT_GT(
        first_read,
        0);

    const ProtocolFrame* decoded_frame =
        nullptr;

    for (std::ptrdiff_t i = 0;
         i < first_read;
         ++i)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                read_buffer[i],
                &decoded_frame));
    }

    ASSERT_EQ(
        ::write(
            master_fd,
            wire_data + first_part_size,
            wire_size - first_part_size),
        static_cast<ssize_t>(
            wire_size - first_part_size));

    const LinuxSerialPollResult second_poll =
        serial_port.wait(
            false,
            100);

    ASSERT_TRUE(
        second_poll.readable);

    const std::ptrdiff_t second_read =
        serial_port.read_some(
            read_buffer,
            sizeof(read_buffer));

    ASSERT_GT(
        second_read,
        0);

    bool frame_completed = false;

    for (std::ptrdiff_t i = 0;
         i < second_read;
         ++i)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                read_buffer[i],
                &decoded_frame))
        {
            frame_completed = true;
        }
    }

    ASSERT_TRUE(
        frame_completed);

    ASSERT_NE(
        decoded_frame,
        nullptr);

    EXPECT_EQ(
        decoded_frame->message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK);

    EXPECT_EQ(
        decoded_frame->sequence,
        42U);

    LinkSyncPayload decoded_payload = {};

    link_sync_decode(
        decoded_frame->payload,
        &decoded_payload);

    EXPECT_EQ(
        decoded_payload.sync_token,
        sync_token);

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    DecodesTwoCombinedFramesFromPtyMaster)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    const LinkSyncPayload first_payload =
    {
        .sync_token =
            UINT64_C(0x1111111111111111)
    };

    ProtocolFrame first_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 10U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &first_payload,
        first_frame.payload);

    const LinkSyncPayload second_payload =
    {
        .sync_token =
            UINT64_C(0x2222222222222222)
    };

    ProtocolFrame second_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 11U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &second_payload,
        second_frame.payload);

    std::uint8_t first_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t second_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t first_size =
        protocol_frame_encode(
            &first_frame,
            first_wire);

    const std::size_t second_size =
        protocol_frame_encode(
            &second_frame,
            second_wire);

    ASSERT_GT(first_size, 0U);
    ASSERT_GT(second_size, 0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        first_wire,
        first_wire + first_size,
        combined);

    std::copy(
        second_wire,
        second_wire + second_size,
        combined + first_size);

    const std::size_t combined_size =
        first_size + second_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    ASSERT_TRUE(
        serial_port.wait(
            false,
            100).readable);

    std::uint8_t read_buffer[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    const std::ptrdiff_t bytes_read =
        serial_port.read_some(
            read_buffer,
            sizeof(read_buffer));

    ASSERT_EQ(
        bytes_read,
        static_cast<std::ptrdiff_t>(
            combined_size));

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    std::uint16_t decoded_sequences[2] = {};
    std::size_t decoded_count = 0U;

    for (std::ptrdiff_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (protocol_frame_decoder_feed_byte(
                &decoder,
                read_buffer[i],
                &frame))
        {
            ASSERT_LT(
                decoded_count,
                2U);

            // Copy what we need immediately.
            // The decoder-owned frame is only valid
            // until the next feed_byte() call.
            decoded_sequences[decoded_count] =
                frame->sequence;

            ++decoded_count;
        }
    }

    ASSERT_EQ(
        decoded_count,
        2U);

    EXPECT_EQ(
        decoded_sequences[0],
        10U);

    EXPECT_EQ(
        decoded_sequences[1],
        11U);

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    RecoversAfterBadCrcFrame)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    const LinkSyncPayload bad_payload =
    {
        .sync_token =
            UINT64_C(0x1111111111111111)
    };

    ProtocolFrame bad_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 20U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &bad_payload,
        bad_frame.payload);

    const LinkSyncPayload good_payload =
    {
        .sync_token =
            UINT64_C(0x2222222222222222)
    };

    ProtocolFrame good_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 21U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &good_payload,
        good_frame.payload);

    std::uint8_t bad_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t good_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t bad_size =
        protocol_frame_encode(
            &bad_frame,
            bad_wire);

    const std::size_t good_size =
        protocol_frame_encode(
            &good_frame,
            good_wire);

    ASSERT_GT(bad_size, 0U);
    ASSERT_GT(good_size, 0U);

    // Deliberately corrupt the CRC.
    bad_wire[bad_size - 1U] ^= 0xFFU;

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        bad_wire,
        bad_wire + bad_size,
        combined);

    std::copy(
        good_wire,
        good_wire + good_size,
        combined + bad_size);

    const std::size_t combined_size =
        bad_size + good_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    std::size_t total_read = 0U;
    bool valid_frame_received = false;
    std::uint16_t decoded_sequence = 0U;

    while (total_read < combined_size)
    {
        ASSERT_TRUE(
            serial_port.wait(
                false,
                100).readable);

        std::uint8_t read_buffer[128] = {};

        const std::ptrdiff_t bytes_read =
            serial_port.read_some(
                read_buffer,
                sizeof(read_buffer));

        ASSERT_GT(
            bytes_read,
            0);

        total_read +=
            static_cast<std::size_t>(
                bytes_read);

        for (std::ptrdiff_t i = 0;
             i < bytes_read;
             ++i)
        {
            const ProtocolFrame* frame =
                nullptr;

            if (protocol_frame_decoder_feed_byte(
                    &decoder,
                    read_buffer[i],
                    &frame))
            {
                valid_frame_received = true;
                decoded_sequence =
                    frame->sequence;
            }
        }
    }

    EXPECT_EQ(
        decoder.crc_error_count,
        1U);

    ASSERT_TRUE(
        valid_frame_received);

    EXPECT_EQ(
        decoded_sequence,
        21U);

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    DetectsPtyMasterHangup)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    // Simulate the remote serial device disappearing.
    ::close(master_fd);
    master_fd = -1;

    const LinuxSerialPollResult poll_result =
        serial_port.wait(
            false,
            100);

    EXPECT_TRUE(
        poll_result.disconnected);

    serial_port.close();
}

TEST(
    LinuxSerialPortPtyTest,
    MotionRetryReusesSameTransaction)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    Stm32MotionSession motion_session;

    constexpr std::uint32_t motion_session_id =
        UINT32_C(1234);

    const auto original =
        motion_session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(original.has_value());

    std::uint8_t original_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t original_wire_size =
        protocol_frame_encode(
            &original.value(),
            original_wire);

    ASSERT_GT(
        original_wire_size,
        0U);

    ASSERT_TRUE(
        serial_port.wait(
            true,
            100).writable);

    ASSERT_EQ(
        serial_port.write_some(
            original_wire,
            original_wire_size),
        static_cast<std::ptrdiff_t>(
            original_wire_size));

    motion_session.mark_pending_transmitted(
        1000U);

    std::uint8_t first_received[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const ssize_t first_received_size =
        ::read(
            master_fd,
            first_received,
            sizeof(first_received));

    ASSERT_GT(
        first_received_size,
        0);

    ProtocolFrameDecoder first_decoder = {};

    protocol_frame_decoder_init(
        &first_decoder);

    ProtocolFrame first_frame_copy = {};
    bool first_complete = false;

    for (ssize_t i = 0;
         i < first_received_size;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (protocol_frame_decoder_feed_byte(
                &first_decoder,
                first_received[i],
                &frame))
        {
            first_frame_copy = *frame;
            first_complete = true;
        }
    }

    ASSERT_TRUE(first_complete);

    EXPECT_FALSE(
        motion_session.retry_if_due(
            1099U).has_value());

    const auto retry =
        motion_session.retry_if_due(
            1100U);

    ASSERT_TRUE(retry.has_value());

    std::uint8_t retry_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t retry_wire_size =
        protocol_frame_encode(
            &retry.value(),
            retry_wire);

    ASSERT_EQ(
        retry_wire_size,
        original_wire_size);

    ASSERT_TRUE(
        serial_port.wait(
            true,
            100).writable);

    ASSERT_EQ(
        serial_port.write_some(
            retry_wire,
            retry_wire_size),
        static_cast<std::ptrdiff_t>(
            retry_wire_size));

    std::uint8_t second_received[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const ssize_t second_received_size =
        ::read(
            master_fd,
            second_received,
            sizeof(second_received));

    ASSERT_GT(
        second_received_size,
        0);

    ProtocolFrameDecoder second_decoder = {};

    protocol_frame_decoder_init(
        &second_decoder);

    ProtocolFrame second_frame_copy = {};
    bool second_complete = false;

    for (ssize_t i = 0;
         i < second_received_size;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (protocol_frame_decoder_feed_byte(
                &second_decoder,
                second_received[i],
                &frame))
        {
            second_frame_copy = *frame;
            second_complete = true;
        }
    }

    ASSERT_TRUE(second_complete);

    EXPECT_EQ(
        second_frame_copy.message_type,
        first_frame_copy.message_type);

    EXPECT_EQ(
        second_frame_copy.sequence,
        first_frame_copy.sequence);

    EXPECT_EQ(
        second_frame_copy.payload_length,
        first_frame_copy.payload_length);

    MotionLifecycleCommandPayload first_payload = {};
    MotionLifecycleCommandPayload second_payload = {};

    motion_lifecycle_command_decode(
        first_frame_copy.payload,
        &first_payload);

    motion_lifecycle_command_decode(
        second_frame_copy.payload,
        &second_payload);

    EXPECT_EQ(
        second_payload.command,
        first_payload.command);

    EXPECT_EQ(
        second_payload.motion_session_id,
        first_payload.motion_session_id);

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    MotionRetriesWhenTerminalResponseIsMissing)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    Stm32MotionSession motion_session;

    const auto request =
        motion_session.begin_start_session(
            UINT32_C(1234));

    ASSERT_TRUE(request.has_value());

    std::uint8_t request_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t request_wire_size =
        protocol_frame_encode(
            &request.value(),
            request_wire);

    ASSERT_GT(request_wire_size, 0U);

    ASSERT_TRUE(
        serial_port.wait(
            true,
            100).writable);

    ASSERT_EQ(
        serial_port.write_some(
            request_wire,
            request_wire_size),
        static_cast<std::ptrdiff_t>(
            request_wire_size));

    motion_session.mark_pending_transmitted(
        1000U);

    /*
     * Fake STM32 consumes the request.
     */
    std::uint8_t fake_stm32_rx[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    ASSERT_GT(
        ::read(
            master_fd,
            fake_stm32_rx,
            sizeof(fake_stm32_rx)),
        0);

    /*
     * Fake STM32 returns only ACK.
     * It deliberately never sends MOTION_RESPONSE.
     */
    const MotionAckPayload ack_payload =
    {
        .status = MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    ASSERT_GT(ack_wire_size, 0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            ack_wire,
            ack_wire_size),
        static_cast<ssize_t>(
            ack_wire_size));

    ASSERT_TRUE(
        serial_port.wait(
            false,
            100).readable);

    std::uint8_t pi_rx[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::ptrdiff_t bytes_read =
        serial_port.read_some(
            pi_rx,
            sizeof(pi_rx));

    ASSERT_GT(bytes_read, 0);

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    bool ack_accepted = false;

    for (std::ptrdiff_t i = 0;
         i < bytes_read;
         ++i)
    {
        const ProtocolFrame* frame =
            nullptr;

        if (protocol_frame_decoder_feed_byte(
                &decoder,
                pi_rx[i],
                &frame))
        {
            ack_accepted =
                motion_session.handle_ack(
                    *frame,
                    1050U);
        }
    }

    ASSERT_TRUE(ack_accepted);

    /*
     * Terminal timeout starts at ACK time:
     * 1050 + 750 = 1800 ms.
     */
    EXPECT_FALSE(
        motion_session.retry_if_due(
            1799U).has_value());

    const auto retry =
        motion_session.retry_if_due(
            1800U);

    ASSERT_TRUE(retry.has_value());

    EXPECT_EQ(
        retry->sequence,
        request->sequence);

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    MotionStopsAfterThreeRetransmissions)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    Stm32MotionSession motion_session;

    const auto original =
        motion_session.begin_start_session(
            UINT32_C(1234));

    ASSERT_TRUE(original.has_value());

    auto transmit =
        [&](const ProtocolFrame& frame)
    {
        std::uint8_t wire[
            PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

        const std::size_t wire_size =
            protocol_frame_encode(
                &frame,
                wire);

        ASSERT_GT(wire_size, 0U);

        ASSERT_TRUE(
            serial_port.wait(
                true,
                100).writable);

        ASSERT_EQ(
            serial_port.write_some(
                wire,
                wire_size),
            static_cast<std::ptrdiff_t>(
                wire_size));

        std::uint8_t fake_stm32_rx[
            PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

        ASSERT_GT(
            ::read(
                master_fd,
                fake_stm32_rx,
                sizeof(fake_stm32_rx)),
            0);
    };

    /*
     * Initial transmission.
     */
    transmit(original.value());
    motion_session.mark_pending_transmitted(0U);

    /*
     * Retransmission 1.
     */
    const auto retry1 =
        motion_session.retry_if_due(100U);

    ASSERT_TRUE(retry1.has_value());
    EXPECT_EQ(retry1->sequence, original->sequence);

    transmit(retry1.value());
    motion_session.mark_pending_transmitted(100U);

    /*
     * Retransmission 2.
     */
    const auto retry2 =
        motion_session.retry_if_due(200U);

    ASSERT_TRUE(retry2.has_value());
    EXPECT_EQ(retry2->sequence, original->sequence);

    transmit(retry2.value());
    motion_session.mark_pending_transmitted(200U);

    /*
     * Retransmission 3.
     */
    const auto retry3 =
        motion_session.retry_if_due(300U);

    ASSERT_TRUE(retry3.has_value());
    EXPECT_EQ(retry3->sequence, original->sequence);

    transmit(retry3.value());
    motion_session.mark_pending_transmitted(300U);

    /*
     * No fourth retransmission.
     */
    EXPECT_FALSE(
        motion_session.retry_if_due(
            400U).has_value());

    EXPECT_TRUE(
        motion_session.retry_exhausted(
            400U));

    serial_port.close();
    ::close(master_fd);
}

TEST(
    LinuxSerialPortPtyTest,
    ProtocolFrameSendFailsWhenWriteDeadlineExpires)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    LinuxSerialPort serial_port;

    ASSERT_TRUE(
        serial_port.open(
            slave_name));

    /*
     * Fill the PTY output buffer without reading
     * from the master until the nonblocking slave
     * can no longer accept data.
     */
    std::uint8_t filler[1024] = {};

    while (true)
    {
        const std::ptrdiff_t written =
            serial_port.write_some(
                filler,
                sizeof(filler));

        ASSERT_GE(written, 0);

        if (written == 0)
        {
            break;
        }
    }

    ProtocolFrame frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence = 1U,

        .payload_length = 0U
    };

    const auto start =
        std::chrono::steady_clock::now();

    EXPECT_FALSE(
        send_protocol_frame(
            serial_port,
            frame,
            std::chrono::milliseconds(100)));

    const auto elapsed =
        std::chrono::steady_clock::now() -
        start;

    EXPECT_LT(
        elapsed,
        std::chrono::milliseconds(500));

    serial_port.close();
    ::close(master_fd);
}

TEST(
    Stm32ClientRunnerPtyTest,
    DoesNotStartMotionAfterSynchronizationWithoutWheelDemand)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * First poll opens the serial port and sends LINK_SYNC.
     */
    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    /*
     * Fake STM32 returns the matching LINK_SYNC_OK.
     */
    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(
        sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    /*
     * Process LINK_SYNC_OK.
     */
    runner.poll();

    /*
     * Run one synchronized iteration with no wheel demand.
     */
    runner.poll();

    pollfd descriptor =
    {
        .fd = master_fd,
        .events = POLLIN,
        .revents = 0
    };

    /*
     * No MOTION_START -- in fact, no outgoing frame at all
     * should be pending this soon after synchronization.
     */
    EXPECT_EQ(
        ::poll(
            &descriptor,
            1,
            0),
        0);

    ::close(master_fd);
}

TEST(
    Stm32ClientRunnerPtyTest,
    FreshNonzeroWheelDemandStartsMotionSession)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(sync_ok_wire_size, 0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    runner.poll();

    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &payload);

    EXPECT_EQ(
        payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    EXPECT_NE(
        payload.motion_session_id,
        0U);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, SendsWheelVelocityAfterSuccessfulMotionStart)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Begin link synchronization.
     */
    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(
        sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    runner.poll();

    /*
     * Fresh application demand should create MOTION_START.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload start_payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &start_payload);

    ASSERT_EQ(
        start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    ASSERT_NE(
        start_payload.motion_session_id,
        0U);

    /*
     * Fake STM32 accepts the lifecycle transaction.
     */
    const MotionAckPayload ack_payload =
    {
        .status =
            MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            start_payload.motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    /*
     * Put ACK and terminal RESPONSE into one PTY write so
     * one runner RX iteration can consume both frames.
     */
    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t response_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    const std::size_t response_wire_size =
        protocol_frame_encode(
            &response,
            response_wire);

    ASSERT_GT(ack_wire_size, 0U);
    ASSERT_GT(response_wire_size, 0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        ack_wire,
        ack_wire + ack_wire_size,
        combined);

    std::copy(
        response_wire,
        response_wire + response_wire_size,
        combined + ack_wire_size);

    const std::size_t combined_size =
        ack_wire_size +
        response_wire_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    /*
     * Process ACK + terminal START response.
     */
    runner.poll();

    /*
     * Now the motion session is confirmed Active.
     * The next iteration may transmit the latest wheel demand.
     */
    runner.poll();

    const ProtocolFrame wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload wheel_payload = {};

    wheel_velocity_payload_decode(
        wheel.payload,
        &wheel_payload);

    EXPECT_EQ(
        wheel_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        wheel_payload.command.left_velocity_mm_s,
        100);

    EXPECT_EQ(
        wheel_payload.command.right_velocity_mm_s,
        100);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, ExplicitZeroCommandEndsActiveMotionSession)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Begin link synchronization.
     */
    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(
        sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    runner.poll();

    /*
     * Fresh application demand should create MOTION_START.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload start_payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &start_payload);

    ASSERT_EQ(
        start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    ASSERT_NE(
        start_payload.motion_session_id,
        0U);

    /*
     * Fake STM32 accepts the lifecycle transaction.
     */
    const MotionAckPayload ack_payload =
    {
        .status =
            MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            start_payload.motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    /*
     * Put ACK and terminal RESPONSE into one PTY write so
     * one runner RX iteration can consume both frames.
     */
    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t response_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    const std::size_t response_wire_size =
        protocol_frame_encode(
            &response,
            response_wire);

    ASSERT_GT(ack_wire_size, 0U);
    ASSERT_GT(response_wire_size, 0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        ack_wire,
        ack_wire + ack_wire_size,
        combined);

    std::copy(
        response_wire,
        response_wire + response_wire_size,
        combined + ack_wire_size);

    const std::size_t combined_size =
        ack_wire_size +
        response_wire_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    /*
     * Process ACK + terminal START response.
     */
    runner.poll();

    /*
     * Now the motion session is confirmed Active.
     * The next iteration may transmit the latest wheel demand.
     */
    runner.poll();

    const ProtocolFrame wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload wheel_payload = {};

    wheel_velocity_payload_decode(
        wheel.payload,
        &wheel_payload);

    EXPECT_EQ(
        wheel_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        wheel_payload.command.left_velocity_mm_s,
        100);

    EXPECT_EQ(
        wheel_payload.command.right_velocity_mm_s,
        100);

    const WheelVelocityCommand zero_command =
    {
        .left_velocity_mm_s = 0,
        .right_velocity_mm_s = 0
    };

    runner.set_wheel_command(
        zero_command);

    runner.poll();

    const ProtocolFrame motion_end =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_end.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload end_payload = {};

    motion_lifecycle_command_decode(
        motion_end.payload,
        &end_payload);

    EXPECT_EQ(
        end_payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION);

    EXPECT_EQ(
        end_payload.motion_session_id,
        start_payload.motion_session_id);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, StaleWheelCommandEndsActiveMotionSession)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Begin link synchronization.
     */
    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(
        sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    runner.poll();

    /*
     * Fresh application demand should create MOTION_START.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload start_payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &start_payload);

    ASSERT_EQ(
        start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    ASSERT_NE(
        start_payload.motion_session_id,
        0U);

    /*
     * Fake STM32 accepts the lifecycle transaction.
     */
    const MotionAckPayload ack_payload =
    {
        .status =
            MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            start_payload.motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    /*
     * Put ACK and terminal RESPONSE into one PTY write so
     * one runner RX iteration can consume both frames.
     */
    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t response_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    const std::size_t response_wire_size =
        protocol_frame_encode(
            &response,
            response_wire);

    ASSERT_GT(ack_wire_size, 0U);
    ASSERT_GT(response_wire_size, 0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        ack_wire,
        ack_wire + ack_wire_size,
        combined);

    std::copy(
        response_wire,
        response_wire + response_wire_size,
        combined + ack_wire_size);

    const std::size_t combined_size =
        ack_wire_size +
        response_wire_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    /*
     * Process ACK + terminal START response.
     */
    runner.poll();

    /*
     * Now the motion session is confirmed Active.
     * The next iteration may transmit the latest wheel demand.
     */
    runner.poll();

    const ProtocolFrame wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload wheel_payload = {};

    wheel_velocity_payload_decode(
        wheel.payload,
        &wheel_payload);

    EXPECT_EQ(
        wheel_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        wheel_payload.command.left_velocity_mm_s,
        100);

    EXPECT_EQ(
        wheel_payload.command.right_velocity_mm_s,
        100);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(210));

    runner.poll();

    const ProtocolFrame motion_end =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_end.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload end_payload = {};

    motion_lifecycle_command_decode(
        motion_end.payload,
        &end_payload);

    EXPECT_EQ(
        end_payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION);

    EXPECT_EQ(
        end_payload.motion_session_id,
        start_payload.motion_session_id);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, LatestWheelCommandWinsOnNextTransmission)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Begin link synchronization.
     */
    runner.poll();

    const ProtocolFrame sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload sync_payload = {};

    link_sync_decode(
        sync.payload,
        &sync_payload);

    ProtocolFrame sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_ok.payload);

    std::uint8_t sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t sync_ok_wire_size =
        protocol_frame_encode(
            &sync_ok,
            sync_ok_wire);

    ASSERT_GT(
        sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            sync_ok_wire,
            sync_ok_wire_size),
        static_cast<ssize_t>(
            sync_ok_wire_size));

    runner.poll();

    /*
     * Fresh application demand should create MOTION_START.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload start_payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &start_payload);

    ASSERT_EQ(
        start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    ASSERT_NE(
        start_payload.motion_session_id,
        0U);

    /*
     * Fake STM32 accepts the lifecycle transaction.
     */
    const MotionAckPayload ack_payload =
    {
        .status =
            MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            start_payload.motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    /*
     * Put ACK and terminal RESPONSE into one PTY write so
     * one runner RX iteration can consume both frames.
     */
    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t response_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    const std::size_t response_wire_size =
        protocol_frame_encode(
            &response,
            response_wire);

    ASSERT_GT(ack_wire_size, 0U);
    ASSERT_GT(response_wire_size, 0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        ack_wire,
        ack_wire + ack_wire_size,
        combined);

    std::copy(
        response_wire,
        response_wire + response_wire_size,
        combined + ack_wire_size);

    const std::size_t combined_size =
        ack_wire_size +
        response_wire_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    /*
     * Process ACK + terminal START response.
     */
    runner.poll();

    /*
     * Now the motion session is confirmed Active.
     * The next iteration may transmit the latest wheel demand.
     */
    runner.poll();

    const ProtocolFrame wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload wheel_payload = {};

    wheel_velocity_payload_decode(
        wheel.payload,
        &wheel_payload);

    EXPECT_EQ(
        wheel_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        wheel_payload.command.left_velocity_mm_s,
        100);

    EXPECT_EQ(
        wheel_payload.command.right_velocity_mm_s,
        100);

    const WheelVelocityCommand newer_command =
    {
        .left_velocity_mm_s = -150,
        .right_velocity_mm_s = 250
    };

    runner.set_wheel_command(
        newer_command);

    /*
     * The previous synchronized iteration already waited up to
     * 50 ms after transmitting the first wheel frame, so the next
     * wheel transmission is eligible now.
     */
    runner.poll();

    const ProtocolFrame newer_wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        newer_wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload newer_payload = {};

    wheel_velocity_payload_decode(
        newer_wheel.payload,
        &newer_payload);

    EXPECT_EQ(
        newer_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        newer_payload.command.left_velocity_mm_s,
        -150);

    EXPECT_EQ(
        newer_payload.command.right_velocity_mm_s,
        250);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, ReconnectDoesNotReplayPreviousWheelDemand)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Initial synchronization.
     */
    runner.poll();

    const ProtocolFrame first_sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        first_sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload first_sync_payload = {};

    link_sync_decode(
        first_sync.payload,
        &first_sync_payload);

    ProtocolFrame first_sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            first_sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &first_sync_payload,
        first_sync_ok.payload);

    std::uint8_t first_sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t first_sync_ok_wire_size =
        protocol_frame_encode(
            &first_sync_ok,
            first_sync_ok_wire);

    ASSERT_GT(
        first_sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            first_sync_ok_wire,
            first_sync_ok_wire_size),
        static_cast<ssize_t>(
            first_sync_ok_wire_size));

    runner.poll();

    /*
     * Wait until the first heartbeat is due.
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(260));

    runner.poll();

    const ProtocolFrame heartbeat =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        heartbeat.message_type,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT);

    /*
     * Fresh motion demand exists immediately before the
     * STM32 reports that the link is unsynchronized.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    const HeartbeatPayload unsynchronized_payload =
    {
        .link_state =
            LINK_STATE_UNSYNCHRONIZED,

        .uptime_ms = 1234U
    };

    ProtocolFrame unsynchronized_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat.sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &unsynchronized_payload,
        unsynchronized_response.payload);

    std::uint8_t unsynchronized_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t unsynchronized_wire_size =
        protocol_frame_encode(
            &unsynchronized_response,
            unsynchronized_wire);

    ASSERT_GT(
        unsynchronized_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            unsynchronized_wire,
            unsynchronized_wire_size),
        static_cast<ssize_t>(
            unsynchronized_wire_size));

    /*
     * This iteration may create MOTION_START before it reads
     * the unsynchronized heartbeat response. The important
     * property is that the resulting disconnect invalidates
     * that demand and lifecycle state.
     */
    runner.poll();

    const ProtocolFrame old_motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        old_motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload old_start_payload = {};

    motion_lifecycle_command_decode(
        old_motion_start.payload,
        &old_start_payload);

    ASSERT_EQ(
        old_start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    /*
     * Allow the scheduled reconnect.
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(510));

    runner.poll();

    const ProtocolFrame second_sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        second_sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload second_sync_payload = {};

    link_sync_decode(
        second_sync.payload,
        &second_sync_payload);

    ProtocolFrame second_sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            second_sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &second_sync_payload,
        second_sync_ok.payload);

    std::uint8_t second_sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t second_sync_ok_wire_size =
        protocol_frame_encode(
            &second_sync_ok,
            second_sync_ok_wire);

    ASSERT_GT(
        second_sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            second_sync_ok_wire,
            second_sync_ok_wire_size),
        static_cast<ssize_t>(
            second_sync_ok_wire_size));

    runner.poll();

    /*
     * Reconnected and synchronized, but the old application
     * wheel request must not be replayed.
     */
    runner.poll();

    pollfd descriptor =
    {
        .fd = master_fd,
        .events = POLLIN,
        .revents = 0
    };

    EXPECT_EQ(
        ::poll(
            &descriptor,
            1,
            0),
        0);

    ::close(master_fd);
}

TEST(Stm32ClientRunnerPtyTest, ActiveMotionIsNotReplayedAfterReconnect)
{
    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};

    ASSERT_EQ(
        ::openpty(
            &master_fd,
            &slave_fd,
            slave_name,
            nullptr,
            nullptr),
        0);

    ::close(slave_fd);

    Stm32ClientRunner runner(
        slave_name);

    /*
     * Initial link synchronization.
     */
    runner.poll();

    const ProtocolFrame first_sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        first_sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload first_sync_payload = {};

    link_sync_decode(
        first_sync.payload,
        &first_sync_payload);

    ProtocolFrame first_sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            first_sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &first_sync_payload,
        first_sync_ok.payload);

    std::uint8_t first_sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t first_sync_ok_wire_size =
        protocol_frame_encode(
            &first_sync_ok,
            first_sync_ok_wire);

    ASSERT_GT(
        first_sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            first_sync_ok_wire,
            first_sync_ok_wire_size),
        static_cast<ssize_t>(
            first_sync_ok_wire_size));

    runner.poll();

    /*
     * Let the first heartbeat become due while there is
     * still no motion demand.
     *
     * This gives us a real pending heartbeat sequence that
     * we can later answer with UNSYNCHRONIZED.
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(260));

    runner.poll();

    const ProtocolFrame heartbeat =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        heartbeat.message_type,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT);

    /*
     * Request real motion.
     */
    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    runner.set_wheel_command(
        command);

    runner.poll();

    const ProtocolFrame motion_start =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        motion_start.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND);

    MotionLifecycleCommandPayload start_payload = {};

    motion_lifecycle_command_decode(
        motion_start.payload,
        &start_payload);

    ASSERT_EQ(
        start_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    ASSERT_NE(
        start_payload.motion_session_id,
        0U);

    /*
     * Fake STM32 accepts MOTION_START.
     */
    const MotionAckPayload ack_payload =
    {
        .status =
            MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &ack_payload,
        ack.payload);

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            start_payload.motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            motion_start.sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    std::uint8_t ack_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    std::uint8_t response_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t ack_wire_size =
        protocol_frame_encode(
            &ack,
            ack_wire);

    const std::size_t response_wire_size =
        protocol_frame_encode(
            &response,
            response_wire);

    ASSERT_GT(
        ack_wire_size,
        0U);

    ASSERT_GT(
        response_wire_size,
        0U);

    std::uint8_t combined[
        PROTOCOL_FRAME_MAX_WIRE_SIZE * 2U] = {};

    std::copy(
        ack_wire,
        ack_wire + ack_wire_size,
        combined);

    std::copy(
        response_wire,
        response_wire + response_wire_size,
        combined + ack_wire_size);

    const std::size_t combined_size =
        ack_wire_size +
        response_wire_size;

    ASSERT_EQ(
        ::write(
            master_fd,
            combined,
            combined_size),
        static_cast<ssize_t>(
            combined_size));

    /*
     * Process ACK + terminal START response.
     * Motion is now Active locally.
     */
    runner.poll();

    /*
     * Before the next Active iteration, make the pending
     * heartbeat response report UNSYNCHRONIZED.
     *
     * The runner will still emit its first wheel frame at
     * the start of that iteration, then consume this response
     * and disconnect/reset the active motion epoch.
     */
    const HeartbeatPayload unsynchronized_payload =
    {
        .link_state =
            LINK_STATE_UNSYNCHRONIZED,

        .uptime_ms = 1234U
    };

    ProtocolFrame unsynchronized_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat.sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &unsynchronized_payload,
        unsynchronized_response.payload);

    std::uint8_t unsynchronized_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t unsynchronized_wire_size =
        protocol_frame_encode(
            &unsynchronized_response,
            unsynchronized_wire);

    ASSERT_GT(
        unsynchronized_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            unsynchronized_wire,
            unsynchronized_wire_size),
        static_cast<ssize_t>(
            unsynchronized_wire_size));

    runner.poll();

    /*
     * Prove motion really reached the wire before the link
     * was invalidated.
     */
    const ProtocolFrame wheel =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        wheel.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY);

    WheelVelocityPayload wheel_payload = {};

    wheel_velocity_payload_decode(
        wheel.payload,
        &wheel_payload);

    EXPECT_EQ(
        wheel_payload.motion_session_id,
        start_payload.motion_session_id);

    EXPECT_EQ(
        wheel_payload.command.left_velocity_mm_s,
        100);

    EXPECT_EQ(
        wheel_payload.command.right_velocity_mm_s,
        100);

    /*
     * Wait for the runner's scheduled reconnect.
     */
    std::this_thread::sleep_for(
        std::chrono::milliseconds(510));

    runner.poll();

    const ProtocolFrame second_sync =
        read_frame_from_pty_master(
            master_fd);

    ASSERT_EQ(
        second_sync.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC);

    LinkSyncPayload second_sync_payload = {};

    link_sync_decode(
        second_sync.payload,
        &second_sync_payload);

    ProtocolFrame second_sync_ok =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            second_sync.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &second_sync_payload,
        second_sync_ok.payload);

    std::uint8_t second_sync_ok_wire[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t second_sync_ok_wire_size =
        protocol_frame_encode(
            &second_sync_ok,
            second_sync_ok_wire);

    ASSERT_GT(
        second_sync_ok_wire_size,
        0U);

    ASSERT_EQ(
        ::write(
            master_fd,
            second_sync_ok_wire,
            second_sync_ok_wire_size),
        static_cast<ssize_t>(
            second_sync_ok_wire_size));

    runner.poll();

    /*
     * We are synchronized again, but no fresh application
     * command was supplied after reconnect.
     */
    runner.poll();

    pollfd descriptor =
    {
        .fd = master_fd,
        .events = POLLIN,
        .revents = 0
    };

    /*
     * The previous 100/100 demand must not cause either a
     * new MOTION_START or a WHEEL_VELOCITY after reconnect.
     */
    EXPECT_EQ(
        ::poll(
            &descriptor,
            1,
            0),
        0);

    ::close(master_fd);
}