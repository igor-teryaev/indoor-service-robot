#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>

extern "C"
{
#include "uart_protocol_transmitter.h"
#include "uart_tx_port.h"
}

namespace
{
    bool uart_tx_port_write_result = true;
    uint32_t uart_tx_port_write_count = 0U;
    std::vector<uint8_t> written_data;
}

extern "C" bool uart_tx_port_write(
    const uint8_t *data,
    size_t size)
{
    ++uart_tx_port_write_count;
    written_data.assign(data, data + size);
    return uart_tx_port_write_result;
}

class UartProtocolTransmitterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        uart_tx_port_write_result = true;
        uart_tx_port_write_count = 0U;
        written_data.clear();
    }
};

TEST_F(UartProtocolTransmitterTest, RejectsNullFrameWithoutAccessingPort)
{
    EXPECT_FALSE(uart_protocol_transmitter_send(nullptr));
    EXPECT_EQ(uart_tx_port_write_count, 0U);
    EXPECT_TRUE(written_data.empty());
}

TEST_F(UartProtocolTransmitterTest, EncodesFrameAndWritesItToPort)
{
    constexpr ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 0x1234U,
        .payload_length = 5U,
        .payload = {0x01U, 0x12U, 0x34U, 0x56U, 0x78U}
    };

    constexpr std::array<uint8_t, 15U> expected = {
        0xA5U, 0x5AU,
        0x01U,
        0x03U,
        0x12U, 0x34U,
        0x00U, 0x05U,
        0x01U, 0x12U, 0x34U, 0x56U, 0x78U,
        0x3CU, 0xDDU
    };

    ASSERT_TRUE(uart_protocol_transmitter_send(&frame));

    ASSERT_EQ(uart_tx_port_write_count, 1U);
    ASSERT_EQ(written_data.size(), expected.size());

    for (size_t i = 0U; i < expected.size(); ++i)
    {
        EXPECT_EQ(written_data[i], expected[i]);
    }
}

TEST_F(UartProtocolTransmitterTest, RejectsOversizedPayloadWithoutAccessingPort)
{
    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 1U,
        .payload_length = PROTOCOL_FRAME_MAX_PAYLOAD_SIZE + 1U
    };

    EXPECT_FALSE(uart_protocol_transmitter_send(&frame));
    EXPECT_EQ(uart_tx_port_write_count, 0U);
    EXPECT_TRUE(written_data.empty());
}

TEST_F(UartProtocolTransmitterTest, PropagatesPortWriteFailure)
{
    constexpr ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 2U,
        .payload_length = 0U
    };

    uart_tx_port_write_result = false;

    EXPECT_FALSE(uart_protocol_transmitter_send(&frame));
    EXPECT_EQ(uart_tx_port_write_count, 1U);
    EXPECT_FALSE(written_data.empty());
}