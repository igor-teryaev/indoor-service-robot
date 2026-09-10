#include <gtest/gtest.h>

#include "safety_state.h"

namespace
{
    struct SafetyCase
    {
        bool estop;
        bool hardware_fault;
        bool expected_safe;
    };

    class SafetyStateSafeTest
        : public ::testing::TestWithParam<SafetyCase>
    {
    };
}

TEST_P(SafetyStateSafeTest, ReportsSafeStateCorrectly)
{
    const auto test_case = GetParam();

    SafetyState state;

    if (test_case.estop)
    {
        ASSERT_EQ(
            state.report_estop(),
            SafetyStateResult::Updated
        );
    }

    if (test_case.hardware_fault)
    {
        ASSERT_EQ(
            state.report_hardware_fault(),
            SafetyStateResult::Updated
        );
    }

    EXPECT_EQ(
        state.safe(),
        test_case.expected_safe
    );
}

INSTANTIATE_TEST_SUITE_P(
    SafetyCombinations,
    SafetyStateSafeTest,
    ::testing::Values(
        SafetyCase{false, false, true},
        SafetyCase{true,  false, false},
        SafetyCase{false, true,  false},
        SafetyCase{true,  true,  false}
    )
);

using SafetyRequest =
    SafetyStateResult (SafetyState::*)();

using SafetyGetter =
    bool (SafetyState::*)() const;

struct SafetyFlagCase
{
    SafetyRequest report;
    SafetyRequest clear;
    SafetyGetter getter;
};

class SafetyFlagTest
    : public ::testing::TestWithParam<SafetyFlagCase>
{
};

TEST_P(SafetyFlagTest, HandlesFlagLifecycle)
{
    const auto test_case = GetParam();

    SafetyState state;

    ASSERT_FALSE(
        (state.*test_case.getter)()
    );

    EXPECT_EQ(
        (state.*test_case.clear)(),
        SafetyStateResult::AlreadyClear
    );
    EXPECT_FALSE(
        (state.*test_case.getter)()
    );

    ASSERT_EQ(
        (state.*test_case.report)(),
        SafetyStateResult::Updated
    );
    ASSERT_TRUE(
        (state.*test_case.getter)()
    );

    EXPECT_EQ(
        (state.*test_case.report)(),
        SafetyStateResult::AlreadyActive
    );
    EXPECT_TRUE(
        (state.*test_case.getter)()
    );

    EXPECT_EQ(
        (state.*test_case.clear)(),
        SafetyStateResult::Updated
    );
    EXPECT_FALSE(
        (state.*test_case.getter)()
    );
}

INSTANTIATE_TEST_SUITE_P(
    SafetyFlags,
    SafetyFlagTest,
    ::testing::Values(
        SafetyFlagCase{
            &SafetyState::report_estop,
            &SafetyState::clear_estop,
            &SafetyState::estop_active
        },
        SafetyFlagCase{
            &SafetyState::report_hardware_fault,
            &SafetyState::clear_hardware_fault,
            &SafetyState::hardware_fault_active
        }
    )
);
