#include "protocol_ingress_router.h"

#include "heartbeat_codec.h"
#include "link_sync_codec.h"
#include "motion_ack_codec.h"
#include "motion_lifecycle_command_codec.h"
#include "motion_response_codec.h"
#include "wheel_velocity_payload_codec.h"

ProtocolIngressRoute protocol_ingress_route(
    const ProtocolFrame* frame)
{
    if (frame == NULL)
    {
        return PROTOCOL_INGRESS_ROUTE_INVALID_ARGUMENT;
    }

    switch (frame->message_type)
    {
        case PROTOCOL_MESSAGE_TYPE_LINK_SYNC:
            if (frame->payload_length != LINK_SYNC_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_LINK_SYNC;

        case PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK:
            if (frame->payload_length != LINK_SYNC_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_LINK_SYNC_OK;

        case PROTOCOL_MESSAGE_TYPE_HEARTBEAT:
            if (frame->payload_length != HEARTBEAT_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_HEARTBEAT;

        case PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND:
            if (frame->payload_length != MOTION_LIFECYCLE_COMMAND_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_MOTION_COMMAND;

        case PROTOCOL_MESSAGE_TYPE_MOTION_ACK:
            if (frame->payload_length != MOTION_ACK_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_MOTION_ACK;

        case PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE:
            if (frame->payload_length != MOTION_RESPONSE_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_MOTION_RESPONSE;

        case PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY:
            if (frame->payload_length != WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE)
            {
                return PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH;
            }

            return PROTOCOL_INGRESS_ROUTE_WHEEL_VELOCITY;

        default:
            return
                PROTOCOL_INGRESS_ROUTE_UNSUPPORTED_MESSAGE_TYPE;
    }
}