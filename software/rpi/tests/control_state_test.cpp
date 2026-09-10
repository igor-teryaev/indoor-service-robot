#include <gtest/gtest.h>

#include "control_state.h"

using ControlRequest =
    ControlRequestResult (ControlState::*)();

struct ControlTransitionCase
{
    ControlRequest setup;
    ControlRequest request;

    ControlRequestResult expected_result;
    ControlAuthority expected_final_authority;
};

class ControlStateTransitionTest
    : public ::testing::TestWithParam<ControlTransitionCase>
{
};

TEST(ControlStateTest, StartsWithNoAuthority)
{
    const ControlState state;

    EXPECT_EQ(
        state.authority(),
        ControlAuthority::None
    );
}

TEST_P(ControlStateTransitionTest, AppliesTransitionRules)
{
    const auto test_case = GetParam();

    ControlState state;

    if (test_case.setup != nullptr)
    {
        ASSERT_EQ(
            (state.*test_case.setup)(),
            ControlRequestResult::Accepted
        );
    }
    else
    {
        ASSERT_EQ(
            state.authority(),
            ControlAuthority::None
        );
    }

    const auto result = (state.*test_case.request)();

    EXPECT_EQ(
        result,
        test_case.expected_result
    );

    EXPECT_EQ(
        state.authority(),
        test_case.expected_final_authority
    );
}

INSTANTIATE_TEST_SUITE_P(
    ControlTransitions,
    ControlStateTransitionTest,
    ::testing::Values(
        ControlTransitionCase{
            nullptr,
            &ControlState::request_manual_control,
            ControlRequestResult::Accepted,
            ControlAuthority::Manual
        },
        ControlTransitionCase{
            nullptr,
            &ControlState::request_autonomous_control,
            ControlRequestResult::Accepted,
            ControlAuthority::Autonomous
        },
        ControlTransitionCase{
            nullptr,
            &ControlState::request_release_control,
            ControlRequestResult::AlreadyReleased,
            ControlAuthority::None
        },

        ControlTransitionCase{
            &ControlState::request_autonomous_control,
            &ControlState::request_manual_control,
            ControlRequestResult::Accepted,
            ControlAuthority::Manual
        },
        ControlTransitionCase{
            &ControlState::request_autonomous_control,
            &ControlState::request_release_control,
            ControlRequestResult::Accepted,
            ControlAuthority::None
        },
        ControlTransitionCase{
            &ControlState::request_autonomous_control,
            &ControlState::request_autonomous_control,
            ControlRequestResult::AlreadyActive,
            ControlAuthority::Autonomous
        },

        ControlTransitionCase{
            &ControlState::request_manual_control,
            &ControlState::request_manual_control,
            ControlRequestResult::AlreadyActive,
            ControlAuthority::Manual
        },
        ControlTransitionCase{
            &ControlState::request_manual_control,
            &ControlState::request_autonomous_control,
            ControlRequestResult::RejectedManualActive,
            ControlAuthority::Manual
        },
        ControlTransitionCase{
            &ControlState::request_manual_control,
            &ControlState::request_release_control,
            ControlRequestResult::Accepted,
            ControlAuthority::None
        }
    )
);
