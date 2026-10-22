#include "crsf_frame_decoder.h"
#include "crsf_crc8.h"
#include <algorithm>

void CrsfFrameDecoder::reset()
{
    state_ = State::WaitingForAddress;
    used_ = 0U;
    expected_size_ = 0U;
}

bool CrsfFrameDecoder::feed_byte(std::uint8_t byte, CrsfFrame& frame)
{
    switch (state_)
    {
    case State::WaitingForAddress:
        if (byte != 0xC8)
        {
            return false;
        }

        buffer_[0] = byte;
        used_ = 1U;
        state_ = State::WaitingForLength;
        return false;
    case State::WaitingForLength:
        if ((byte >= 2U) && (byte <= buffer_.size() - 2U))
        {
            buffer_[1] = byte;
            used_ = 2U;
            expected_size_ = byte + 2U;
            state_ = State::Collecting;
        }
        else
        {
            reset();

            if (byte == 0xC8)
            {
                buffer_[0] = byte;
                used_ = 1U;
                state_ = State::WaitingForLength;
            }
            return false;
        }
        return false;

    case State::Collecting:
        {
            buffer_[used_] = byte;
            used_++;

            if (used_ < expected_size_)
            {
                return false;
            }

            std::uint8_t calculated_crc = 0;
            for (std::size_t i = 2; i <= expected_size_ - 2; ++i) {
                calculated_crc = crsf_crc8_update(calculated_crc, buffer_[i]);
            }

            if (calculated_crc == buffer_[expected_size_ - 1])
            {
                CrsfFrame local_frame = {
                    .type = buffer_[2],
                    .payload_length = expected_size_ - 4U
                };

                std::copy(
                            buffer_.begin() + 3,
                            buffer_.begin() + 3 + local_frame.payload_length,
                            local_frame.payload.begin());
                frame = local_frame;
                reset();
                return true;
            }

            reset();
        }
    default:
        return false;
    }
}