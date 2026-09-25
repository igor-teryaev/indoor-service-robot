#include <gtest/gtest.h>

extern "C"
{
#include "uart_rx_queue.h"
}

TEST(UART_RX_QueueTest, PreservesByteOrder)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);
    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x01));
    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x02));
    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x03));

    uint8_t byte = 0x00;
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x01, byte);
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x02, byte);
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x03, byte);

    EXPECT_FALSE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_FALSE(uart_rx_queue_has_overflowed(&queue));
}

TEST(UART_RX_QueueTest, LatchesOverflowAndBlocksAccess)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);

    for (uint32_t i = 0U; i < UART_RX_QUEUE_CAPACITY; ++i)
    {
        ASSERT_TRUE(uart_rx_queue_push(&queue, static_cast<uint8_t>(i)));
    }

    EXPECT_FALSE(uart_rx_queue_has_overflowed(&queue));
    EXPECT_FALSE(uart_rx_queue_push(&queue, 0x01));
    EXPECT_TRUE(uart_rx_queue_has_overflowed(&queue));

    uint8_t byte = 0x00;
    EXPECT_FALSE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_FALSE(uart_rx_queue_push(&queue, 0x02));
}

TEST(UART_RX_QueueTest, ResetDiscardsBufferedBytesAndAllowsReuse)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);

    for (uint32_t i = 0U; i < UART_RX_QUEUE_CAPACITY; ++i)
    {
        ASSERT_TRUE(uart_rx_queue_push(&queue, static_cast<uint8_t>(i)));
    }
    EXPECT_FALSE(uart_rx_queue_push(&queue, 0x01));
    EXPECT_TRUE(uart_rx_queue_has_overflowed(&queue));

    uart_rx_queue_reset(&queue);
    uint8_t byte = 0x01;

    EXPECT_FALSE(uart_rx_queue_has_overflowed(&queue));

    EXPECT_FALSE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x01, byte);

    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x05));
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x05, byte);
}

TEST(UART_RX_QueueTest, PreservesOrderAcrossCounterWraparound)
{
    UartRxQueue queue = {0};
    uart_rx_queue_init(&queue);
    queue.read_count = UINT32_MAX - 1U;
    queue.write_count = UINT32_MAX - 1U;

    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x01));
    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x02));
    EXPECT_TRUE(uart_rx_queue_push(&queue, 0x03));

    uint8_t byte = 0x00;
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x01, byte);
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x02, byte);
    EXPECT_TRUE(uart_rx_queue_pop(&queue, &byte));
    EXPECT_EQ(0x03, byte);

    EXPECT_FALSE(uart_rx_queue_has_overflowed(&queue));
    EXPECT_EQ(queue.read_count, queue.write_count);
}