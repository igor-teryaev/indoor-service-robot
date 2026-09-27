#pragma once

#include <string>
#include <cstddef>
#include <cstdint>

struct LinuxSerialPollResult
{
    bool readable = false;
    bool writable = false;
    bool disconnected = false;
};

class LinuxSerialPort
{
public:
    LinuxSerialPort() = default;
    ~LinuxSerialPort();

    LinuxSerialPort(const LinuxSerialPort&) = delete;

    LinuxSerialPort& operator=(const LinuxSerialPort&) = delete;

    [[nodiscard]] std::ptrdiff_t read_some(std::uint8_t* buffer, std::size_t capacity);

    [[nodiscard]] std::ptrdiff_t write_some(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] LinuxSerialPollResult wait(bool watch_write, int timeout_ms);

    [[nodiscard]] bool open(const std::string& device_path);

    void close();

    [[nodiscard]] bool is_open() const;

private:
    int fd_ = -1;
};