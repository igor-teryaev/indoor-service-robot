#include <gtest/gtest.h>

#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include "linux_serial_port.h"

class LinuxSerialPortConfigTest : public ::testing::Test
{
protected:
    int master_fd_ = -1;
    int observer_fd_ = -1;
    std::string slave_path_;

    void SetUp() override
    {
        master_fd_ = ::posix_openpt(O_RDWR | O_NOCTTY);
        ASSERT_GE(master_fd_, 0);
        ASSERT_EQ(::grantpt(master_fd_), 0);
        ASSERT_EQ(::unlockpt(master_fd_), 0);

        const char* slave_name = ::ptsname(master_fd_);
        ASSERT_NE(slave_name, nullptr);
        slave_path_ = slave_name;
    }

    void TearDown() override
    {
        if (observer_fd_ >= 0)
        {
            ::close(observer_fd_);
        }
        if (master_fd_ >= 0)
        {
            ::close(master_fd_);
        }
    }
};

TEST_F(LinuxSerialPortConfigTest, AppliesCustomBaudRate)
{
    LinuxSerialPort serial_port;
    ASSERT_TRUE(serial_port.open(slave_path_, 420000U));

    observer_fd_ = ::open(
        slave_path_.c_str(),
        O_RDONLY | O_NOCTTY | O_NONBLOCK);
    ASSERT_GE(observer_fd_, 0);

    termios2 tty{};
    ASSERT_EQ(::ioctl(observer_fd_, TCGETS2, &tty), 0);

    EXPECT_EQ(tty.c_ispeed, 420000U);
    EXPECT_EQ(tty.c_ospeed, 420000U);
    EXPECT_EQ(tty.c_cflag & CBAUD, BOTHER);
}
