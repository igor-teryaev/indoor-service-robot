#include "protocol_frame_encoder.h"

#include <string.h>

#include "byte_codec.h"
#include "crc16_ccitt_false.h"

size_t protocol_frame_encode(
    const ProtocolFrame* frame,
    uint8_t* output)
{
    if (frame->payload_length > PROTOCOL_FRAME_MAX_PAYLOAD_SIZE)
    {
        return 0U;
    }

    output[PROTOCOL_FRAME_MAGIC_0_OFFSET] = PROTOCOL_FRAME_MAGIC_0;
    output[PROTOCOL_FRAME_MAGIC_1_OFFSET] = PROTOCOL_FRAME_MAGIC_1;
    output[PROTOCOL_FRAME_VERSION_OFFSET] = PROTOCOL_FRAME_VERSION;
    output[PROTOCOL_FRAME_MESSAGE_TYPE_OFFSET] = frame->message_type;
    byte_codec_encode_u16_be(frame->sequence, &output[PROTOCOL_FRAME_SEQUENCE_OFFSET]);
    byte_codec_encode_u16_be(frame->payload_length, &output[PROTOCOL_FRAME_PAYLOAD_LENGTH_OFFSET]);

    if (frame->payload_length > 0U)
    {
        memcpy(
            &output[PROTOCOL_FRAME_PAYLOAD_OFFSET],
            frame->payload,
            frame->payload_length
        );
    }

    const uint8_t* crc_data = output + PROTOCOL_FRAME_VERSION_OFFSET;
    const size_t crc_offset = PROTOCOL_FRAME_PAYLOAD_OFFSET + frame->payload_length;

    const size_t crc_data_length =
        crc_offset - PROTOCOL_FRAME_VERSION_OFFSET;

    const uint16_t crc = crc16_ccitt_false(crc_data, crc_data_length);
    byte_codec_encode_u16_be(crc, &output[crc_offset]);

    const size_t frame_size =
        PROTOCOL_FRAME_HEADER_SIZE +
        frame->payload_length +
        PROTOCOL_FRAME_CRC_SIZE;
    return frame_size;
}
