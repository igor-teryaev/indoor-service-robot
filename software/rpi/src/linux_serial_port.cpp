#include "linux_serial_port.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <cerrno>
#include <chrono>

LinuxSerialPort::~LinuxSerialPort()
{
    close();
}

bool LinuxSerialPort::open(
    const std::string& device_path)
{
    close();

    fd_ = ::open(
        device_path.c_str(),
        O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd_ < 0)
    {
        return false;
    }

    termios tty = {};

    if (tcgetattr(fd_, &tty) != 0)
    {
        close();
        return false;
    }

    cfmakeraw(&tty);

    if (cfsetispeed(&tty, B115200) != 0 ||
        cfsetospeed(&tty, B115200) != 0)
    {
        close();
        return false;
    }

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;

    tty.c_cflag &= ~CRTSCTS;

    tty.c_cflag |= CLOCAL;
    tty.c_cflag |= CREAD;

    if (tcsetattr(
            fd_,
            TCSANOW,
            &tty) != 0)
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