#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct CrsfFrame
{
    std::uint8_t type = 0U;
    std::array<std::uint8_t, 60> payload{};
    std::size_t payload_length = 0U;
};

class CrsfFrameDecoder
{
public:
    bool feed_byte(std::uint8_t byte, CrsfFrame& frame);
    void reset();

private:
    enum class State
    {
        WaitingForAddress,
        WaitingForLength,
        Collecting
    };

    State state_ = State::WaitingForAddress;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t used_ = 0U;
    std::size_t expected_size_ = 0U;
};