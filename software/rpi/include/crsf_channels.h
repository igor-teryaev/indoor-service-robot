#pragma once

#include <array>
#include <cstdint>

#include "crsf_frame_decoder.h"

struct CrsfChannels
{
    std::array<std::uint16_t, 16> values{};
};

bool crsf_channels_decode(const CrsfFrame& frame, CrsfChannels& channels);