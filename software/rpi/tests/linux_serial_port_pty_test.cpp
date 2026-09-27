#include <gtest/gtest.h>

#include <pty.h>
#include <unistd.h>

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