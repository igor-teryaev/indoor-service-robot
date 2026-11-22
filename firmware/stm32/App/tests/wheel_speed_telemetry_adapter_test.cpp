#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "wheel_speed_telemetry_adapter.h"

TEST(WheelSpeedTelemetryAdapterTest, RoundsSpeedsToNearestInteger)
{
    const WheelSpeedEstimate estimate{
        .left_mm_s = 120.4f,
        .right_mm_s = 120.5f,
        .valid = true
    };

    const auto payload =
        wheel_speed_telemetry_payload_from_estimate(&estimate);

    EXPECT_TRUE(payload.valid);
    EXPECT_EQ(payload.left_speed_mm_s, 120U);
    EXPECT_EQ(payload.right_speed_mm_s, 121U);
}

TEST(WheelSpeedTelemetryAdapterTest, SaturatesSpeedsAtUint16Max)
{
    const WheelSpeedEstimate estimate{
        .left_mm_s = 70000.0f,
        .right_mm_s = 65535.0f,
        .valid = true
    };

    const auto payload =
        wheel_speed_telemetry_payload_from_estimate(&estimate);

    EXPECT_TRUE(payload.valid);
    EXPECT_EQ(payload.left_speed_mm_s, UINT16_MAX);
    EXPECT_EQ(payload.right_speed_mm_s, UINT16_MAX);
}

TEST(WheelSpeedTelemetryAdapterTest, RejectsInvalidMeasurements)
{
    const WheelSpeedEstimate estimates[]{
        {120.0f, 100.0f, false},
        {std::numeric_limits<float>::quiet_NaN(), 100.0f, true},
        {100.0f, std::numeric_limits<float>::infinity(), true},
        {-1.0f, 100.0f, true},
        {100.0f, -1.0f, true}
    };

    for (const auto& estimate : estimates)
    {
        const auto payload =
            wheel_speed_telemetry_payload_from_estimate(&estimate);

        EXPECT_FALSE(payload.valid);
        EXPECT_EQ(payload.left_speed_mm_s, 0U);
        EXPECT_EQ(payload.right_speed_mm_s, 0U);
    }
}

TEST(WheelSpeedTelemetryAdapterTest, RejectsNullEstimate)
{
    const auto payload =
        wheel_speed_telemetry_payload_from_estimate(nullptr);

    EXPECT_FALSE(payload.valid);
    EXPECT_EQ(payload.left_speed_mm_s, 0U);
    EXPECT_EQ(payload.right_speed_mm_s, 0U);
}