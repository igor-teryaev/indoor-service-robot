#include "wheel_speed_telemetry_payload_codec.h"

#include "byte_codec.h"

void wheel_speed_telemetry_payload_encode(
    const WheelSpeedTelemetryPayload* payload,
    uint8_t output[WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE])
{
    byte_codec_encode_u16_be(
        payload->left_speed_mm_s,
        output);

    byte_codec_encode_u16_be(
        payload->right_speed_mm_s,
        output + 2);

    output[4] = payload->valid ? 1U : 0U;
}

void wheel_speed_telemetry_payload_decode(
    const uint8_t input[WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE],
    WheelSpeedTelemetryPayload* payload)
{
    payload->left_speed_mm_s =
        byte_codec_decode_u16_be(input);

    payload->right_speed_mm_s =
        byte_codec_decode_u16_be(input + 2);

    payload->valid = input[4] != 0U;
}