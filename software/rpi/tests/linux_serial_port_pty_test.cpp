#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include "linux_serial_port.h"

extern "C"
{
#include "link_sync_codec.h"
#include "protocol_frame_decoder.h"
#include "protocol_frame_encoder.h"
#include "protocol_message_type.h"
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