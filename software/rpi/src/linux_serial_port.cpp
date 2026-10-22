#include "linux_serial_port.h"

#include <fcntl.h>
#include <unistd.h>
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cerrno>
#include <chrono>

LinuxSerialPort::~LinuxSerialPort()
{
    close();
}

bool LinuxSerialPort::open(
    const std::string& device_path,
    std::uint32_t baud_rate)
{
    close();

    if (baud_rate == 0U)
    {
        return false;
    }

    fd_ = ::open(
        device_path.c_str(),
        O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd_ < 0)
    {
        return false;
    }

    termios2 tty{};

    if (::ioctl(fd_, TCGETS2, &tty) != 0)
    {
        close();
        return false;
    }

    tty.c_iflag = 0U;
    tty.c_oflag = 0U;
    tty.c_lflag = 0U;

    // BOTHER selects the numeric rates in c_ispeed/c_ospeed.
    tty.c_cflag = BOTHER | CS8 | CLOCAL | CREAD;
    tty.c_ispeed = baud_rate;
    tty.c_ospeed = baud_rate;

    tty.c_cc[VMIN] = 1U;
    tty.c_cc[VTIME] = 0U;

    if (::ioctl(fd_, TCSETS2, &tty) != 0)
    {
        close();
        return false;
    }

    return true;
}

void LinuxSerialPort::close()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

bool LinuxSerialPort::is_open() const
{
    return fd_ >= 0;
}

std::ptrdiff_t LinuxSerialPort::read_some(
    std::uint8_t* buffer,
    std::size_t capacity)
{
    if (fd_ < 0)
    {
        return -1;
    }

    while (true)
    {
        const ssize_t result =
            ::read(
                fd_,
                buffer,
                capacity);

        if (result > 0)
        {
            return static_cast<std::ptrdiff_t>(result);
        }

        if (result == 0)
        {
            return -1;
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK)
        {
            return 0;
        }

        return -1;
    }
}

std::ptrdiff_t LinuxSerialPort::write_some(
    const std::uint8_t* data,
    std::size_t size)
{
    if (fd_ < 0)
    {
        return -1;
    }

    while (true)
    {
        const ssize_t result =
            ::write(
                fd_,
                data,
                size);

        if (result >= 0)
        {
            return static_cast<std::ptrdiff_t>(
                result);
        }

        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK)
        {
            return 0;
        }

        return -1;
    }
}

LinuxSerialPollResult LinuxSerialPort::wait(
    bool watch_write,
    int timeout_ms)
{
    if (fd_ < 0)
    {
        return {
            .disconnected = true
        };
    }

    pollfd descriptor =
    {
        .fd = fd_,
        .events = POLLIN,
        .revents = 0
    };

    if (watch_write)
    {
        descriptor.events |= POLLOUT;
    }

    const auto deadline =
    std::chrono::steady_clock::now() +
    std::chrono::milliseconds(timeout_ms);

    while (true)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= deadline)
        {
            return {};
        }

        const auto remaining =
            std::chrono::ceil<std::chrono::milliseconds>(
                deadline - now);

        const int result =
            ::poll(
                &descriptor,
                1,
                static_cast<int>(
                    remaining.count()));

        if (result > 0)
        {
            LinuxSerialPollResult poll_result;

            poll_result.readable =
                (descriptor.revents & POLLIN) != 0;

            poll_result.writable =
                (descriptor.revents & POLLOUT) != 0;

            poll_result.disconnected =
                (descriptor.revents &
                    (POLLERR |
                     POLLHUP |
                     POLLNVAL)) != 0;

            return poll_result;
        }

        if (result == 0)
        {
            return {};
        }

        if (errno == EINTR)
        {
            continue;
        }

        return {
            .disconnected = true
        };
    }
}