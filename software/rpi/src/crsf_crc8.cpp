#include "crsf_crc8.h"

std::uint8_t crsf_crc8_update(std::uint8_t crc, std::uint8_t byte)
{
    // 1. XOR byte into crc
    crc ^= byte;

    // 2. Repeat eight times
    for (int i = 0; i < 8; ++i) {
        // Inspect bit 7 before shifting
        if (crc & 0x80) {
            // Shift left and if that bit was set, XOR with 0xD5
            crc = static_cast<std::uint8_t>((crc << 1U) ^ 0xD5U);
        } else {
            // Just shift left
            crc = static_cast<std::uint8_t>(crc << 1U);
        }
    }

    // 3. Keep the result within eight bits and return it
    // (Повернення типу std::uint8_t автоматично обрізає значення до 8 біт)
    return crc;
}