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

    bool telemetry_tx_result = true;
    uint32_t telemetry_tx_count = 0U;
    std::vector<uint8_t> telemetry_written_data;
}

extern "C" bool uart_tx_port_write(
    const uint8_t *data,
    size_t size)
{
    ++uart_tx_port_write_count;
    written_data.assign(data, data + size);
    return uart_tx_port_write_result;
}

extern "C" bool uart_tx_port_try_write_telemetry(
    const uint8_t *data,
    size_t size)
{
    ++telemetry_tx_count;
    telemetry_written_data.assign(data, data + size);
    return telemetry_tx_result;
}

class UartProtocolTransmitterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        uart_tx_port_write_result = true;
        uart_tx_port_write_count = 0U;
        written_data.clear();

        telemetry_tx_result = true;
        telemetry_tx_count = 0U;
        telemetry_written_data.clear();
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

TEST_F(UartProtocolTransmitterTest, EncodesAndStartsTelemetryTransmission)
{
    const ProtocolFrame frame{
        .message_type = PROTOCOL_MESSAGE_TYPE_WHEEL_SPEED_TELEMETRY,
        .sequence = 42U,
        .payload_length = 5U,
        .payload = {0x01U, 0x2CU, 0x00U, 0x7DU, 0x01U}
    };

    const std::array<uint8_t, 15> expected{
        0xA5U, 0x5AU,
        0x01U, 0x40U,
        0x00U, 0x2AU,
        0x00U, 0x05U,
        0x01U, 0x2CU,
        0x00U, 0x7DU,
        0x01U,
        0x6BU, 0x40U
    };

    ASSERT_TRUE(
        uart_protocol_transmitter_try_send_telemetry(&frame));

    EXPECT_EQ(telemetry_tx_count, 1U);
    ASSERT_EQ(telemetry_written_data.size(), expected.size());

    EXPECT_TRUE(std::equal(
        expected.begin(),
        expected.end(),
        telemetry_written_data.begin()));
}

TEST_F(UartProtocolTransmitterTest, DropsTelemetryWhenPortIsBusy)
{
    const ProtocolFrame frame{
        .message_type = PROTOCOL_MESSAGE_TYPE_WHEEL_SPEED_TELEMETRY,
        .sequence = 42U,
        .payload_length = 0U
    };

    telemetry_tx_result = false;

    EXPECT_FALSE(
        uart_protocol_transmitter_try_send_telemetry(&frame));

    EXPECT_EQ(telemetry_tx_count, 1U);
    EXPECT_EQ(uart_tx_port_write_count, 0U);
}

TEST_F(UartProtocolTransmitterTest, RejectsNullTelemetryFrame)
{
    EXPECT_FALSE(
        uart_protocol_transmitter_try_send_telemetry(nullptr));

    EXPECT_EQ(telemetry_tx_count, 0U);
}