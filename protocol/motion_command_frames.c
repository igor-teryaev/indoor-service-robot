#include "motion_command_frames.h"

#include "motion_ack_codec.h"
#include "motion_ack_payload.h"
#include "motion_response_codec.h"
#include "motion_response_payload.h"
#include "protocol_message_type.h"

MotionCommandFrames motion_command_frames_build(
    const MotionLifecycleAction* action)
{
    MotionCommandFrames frames = {0};

    if (action->send_ack)
    {
        MotionAckPayload payload = {
            .status = MOTION_ACK_ACCEPTED
        };

        frames.ack_frame.message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK;

        frames.ack_frame.sequence =
            action->ack_transaction.sequence;

        frames.ack_frame.payload_length =
            MOTION_ACK_WIRE_SIZE;

        motion_ack_encode(
            &payload,
            frames.ack_frame.payload
        );

        frames.ack_valid = true;
    }

    if (action->send_response)
    {
        const MotionResponsePayload payload = {
            .command =
                action->response_transaction.command,

            .motion_session_id =
                action->response_transaction.motion_session_id,

            .result =
                action->response_result
        };

        frames.response_frame.message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE;

        frames.response_frame.sequence =
            action->response_transaction.sequence;

        frames.response_frame.payload_length =
            MOTION_RESPONSE_WIRE_SIZE;

        motion_response_encode(
            &payload,
            frames.response_frame.payload
        );

        frames.response_valid = true;
    }

    return frames;
}