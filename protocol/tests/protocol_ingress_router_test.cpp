#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "heartbeat_codec.h"
#include "link_sync_codec.h"
#include "motion_ack_codec.h"
#include "motion_lifecycle_command_codec.h"
#include "motion_response_codec.h"
#include "protocol_ingress_router.h"
#include "protocol_message_type.h"
#include "wheel_velocity_payload_codec.h"

namespace
{

struct RouteCase
{
    ProtocolMessageType message_type;
    uint16_t payload_length;
    ProtocolIngressRoute expected_route;
};

constexpr std::array<RouteCase, 7U> SUPPORTED_ROUTES{{
    {
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        LINK_SYNC_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_LINK_SYNC
    },
    {
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,
        LINK_SYNC_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_LINK_SYNC_OK
    },
    {
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        HEARTBEAT_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_HEARTBEAT
    },
    {
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND,
        MOTION_LIFECYCLE_COMMAND_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_MOTION_COMMAND
    },
    {
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK,
        MOTION_ACK_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_MOTION_ACK
    },
    {
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,
        MOTION_RESPONSE_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_MOTION_RESPONSE
    },
    {
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY,
        WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE,
        PROTOCOL_INGRESS_ROUTE_WHEEL_VELOCITY
    }
}};

ProtocolFrame make_frame(
    ProtocolMessageType message_type,
    uint16_t payload_length)
{
    ProtocolFrame frame{};

    frame.message_type = message_type;
    frame.payload_length = payload_length;

    return frame;
}

}

TEST(ProtocolIngressRouterTest, RoutesSupportedMessageTypes)
{
    for (const RouteCase& test_case : SUPPORTED_ROUTES)
    {
        const ProtocolFrame frame =
            make_frame(
                test_case.message_type,
                test_case.payload_length
            );

        EXPECT_EQ(
            protocol_ingress_route(&frame),
            test_case.expected_route
        );
    }
}

TEST(ProtocolIngressRouterTest, RejectsTooShortPayload)
{
    for (const RouteCase& test_case : SUPPORTED_ROUTES)
    {
        ASSERT_GT(test_case.payload_length, 0U);

        const ProtocolFrame frame =
            make_frame(
                test_case.message_type,
                static_cast<uint16_t>(
                    test_case.payload_length - 1U
                )
            );

        EXPECT_EQ(
            protocol_ingress_route(&frame),
            PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH
        );
    }
}

TEST(ProtocolIngressRouterTest, RejectsTooLongPayload)
{
    for (const RouteCase& test_case : SUPPORTED_ROUTES)
    {
        const ProtocolFrame frame =
            make_frame(
                test_case.message_type,
                static_cast<uint16_t>(
                    test_case.payload_length + 1U
                )
            );

        EXPECT_EQ(
            protocol_ingress_route(&frame),
            PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH
        );
    }
}

TEST(ProtocolIngressRouterTest, RejectsUnsupportedMessageTypes)
{
    const std::array<ProtocolMessageType, 4U> unsupported_types{
        PROTOCOL_MESSAGE_TYPE_RPC_COMMAND,
        PROTOCOL_MESSAGE_TYPE_RPC_ACK,
        PROTOCOL_MESSAGE_TYPE_RPC_RESPONSE,
        0xFFU
    };

    for (const ProtocolMessageType message_type :
         unsupported_types)
    {
        const ProtocolFrame frame =
            make_frame(message_type, 0U);

        EXPECT_EQ(
            protocol_ingress_route(&frame),
            PROTOCOL_INGRESS_ROUTE_UNSUPPORTED_MESSAGE_TYPE
        );
    }
}

TEST(ProtocolIngressRouterTest, RejectsNullFrame)
{
    EXPECT_EQ(
        protocol_ingress_route(nullptr),
        PROTOCOL_INGRESS_ROUTE_INVALID_ARGUMENT
    );
}