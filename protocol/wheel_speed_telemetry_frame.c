#include "wheel_speed_telemetry_frame.h"

#include "protocol_message_type.h"
#include "wheel_speed_telemetry_payload_codec.h"

ProtocolFrame wheel_speed_telemetry_frame_build(
    const WheelSpeedTelemetryPayload *payload,
    uint16_t sequence)
{
    ProtocolFrame frame = {0};

    frame.message_type =
        PROTOCOL_MESSAGE_TYPE_WHEEL_SPEED_TELEMETRY;

    frame.sequence = sequence;

    frame.payload_length =
        WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE;

    wheel_speed_telemetry_payload_encode(
        payload,
        frame.payload);

    return frame;
}