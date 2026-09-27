#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include "linux_serial_port.h"

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