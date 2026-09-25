#include <gtest/gtest.h>

extern "C"
{
#include "protocol_frame_encoder.h"
#include "uart_protocol_receiver.h"
#include "wheel_velocity_payload_codec.h"
}

TEST(uart_protocol_receiver_tests, DecodesCompleteFrameFromQueuedBytes)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);

    UartProtocolReceiver receiver = {nullptr};
    ASSERT_TRUE(uart_protocol_receiver_init(&receiver, &queue));

    constexpr ProtocolFrame input_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY,
        .sequence = 0x1234U,
        .payload_length = WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE,
        .payload = {0x01U, 0x02U, 0x03U, 0x04U,
                    0x05U, 0x06U, 0x07U, 0x08U}
    };

    uint8_t wire[PROTOCOL_FRAME_MAX_WIRE_SIZE] = {0};

    const size_t wire_size = protocol_frame_encode(&input_frame, wire);
    ASSERT_GT(wire_size, 0U);

    for (size_t i = 0; i < wire_size; i++)
    {
        ASSERT_TRUE(uart_rx_queue_push(&queue, wire[i]));
    }

    const ProtocolFrame* output_frame = nullptr;
    ASSERT_EQ(uart_protocol_receiver_poll(&receiver, &output_frame), UART_PROTOCOL_RECEIVER_RESULT_FRAME);
    ASSERT_NE(output_frame, nullptr);

    EXPECT_EQ(output_frame->message_type, input_frame.message_type);
    EXPECT_EQ(output_frame->sequence, input_frame.sequence);
    EXPECT_EQ(output_frame->payload_length, input_frame.payload_length);
    for (size_t i = 0U; i < input_frame.payload_length; ++i)
    {
        EXPECT_EQ(
            output_frame->payload[i],
            input_frame.payload[i]);
    }

    EXPECT_EQ(uart_protocol_receiver_poll(&receiver, &output_frame), UART_PROTOCOL_RECEIVER_RESULT_NONE);
    EXPECT_EQ(output_frame, nullptr);
}

TEST(uart_protocol_receiver_tests, OverflowDiscardsPartialFrameAndRecovers)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);

    UartProtocolReceiver receiver = {nullptr};
    ASSERT_TRUE(uart_protocol_receiver_init(&receiver, &queue));

    constexpr ProtocolFrame input_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY,
        .sequence = 0x1234U,
        .payload_length = WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE,
        .payload = {0x01U, 0x02U, 0x03U, 0x04U,
                    0x05U, 0x06U, 0x07U, 0x08U}
    };

    uint8_t wire[PROTOCOL_FRAME_MAX_WIRE_SIZE] = {0};

    const size_t wire_size = protocol_frame_encode(&input_frame, wire);
    ASSERT_GT(wire_size, 0U);

    for (size_t i = 0; i < 6U; i++)
    {
        ASSERT_TRUE(uart_rx_queue_push(&queue, wire[i]));
    }

    const ProtocolFrame* output_frame = nullptr;
    EXPECT_EQ(uart_protocol_receiver_poll(&receiver, &output_frame), UART_PROTOCOL_RECEIVER_RESULT_NONE);

    for (size_t i = 0; i < UART_RX_QUEUE_CAPACITY; i++)
    {
        ASSERT_TRUE(
            uart_rx_queue_push(
                &queue,
                static_cast<uint8_t>(i)));
    }

    EXPECT_FALSE(uart_rx_queue_push(&queue, 0x40));
    EXPECT_TRUE(uart_rx_queue_has_overflowed(&queue));
    EXPECT_EQ(output_frame, nullptr);

    EXPECT_EQ(uart_protocol_receiver_poll(&receiver, &output_frame), UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW);
    EXPECT_EQ(output_frame, nullptr);
    EXPECT_FALSE(uart_rx_queue_has_overflowed(&queue));

    for (size_t i = 0; i < wire_size; i++)
    {
        ASSERT_TRUE(uart_rx_queue_push(&queue, wire[i]));
    }
    ASSERT_EQ(uart_protocol_receiver_poll(&receiver, &output_frame), UART_PROTOCOL_RECEIVER_RESULT_FRAME);
    ASSERT_NE(output_frame, nullptr);

    EXPECT_EQ(output_frame->sequence, input_frame.sequence);
}