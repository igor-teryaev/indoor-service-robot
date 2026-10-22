#include "crsf_channels.h"

bool crsf_channels_decode(const CrsfFrame& frame, CrsfChannels& channels)
{
    if ((frame.type != 0x16) ||
        (frame.payload_length != 22U))
    {
        return false;
    }

    std::uint32_t bits = 0U;
    unsigned available_bits = 0U;
    std::size_t byte_index = 0U;

    std::array<std::uint16_t, 16> temp_values{};

    for (std::size_t i = 0; i < temp_values.size(); ++i) {
        // Додаємо байти в акумулятор, поки в ньому не буде принаймні 11 бітів
        while (available_bits < 11) {
            bits |= static_cast<std::uint32_t>(frame.payload[byte_index++]) << available_bits;
            available_bits += 8;
        }

        // Маскуємо нижні 11 бітів (0x7FF) та записуємо значення каналу
        temp_values[i] = static_cast<std::uint16_t>(bits & 0x07FF);

        // Зсуваємо використані біти та оновлюємо лічильник
        bits >>= 11;
        available_bits -= 11;
    }

    channels.values = temp_values;
    return true;
}