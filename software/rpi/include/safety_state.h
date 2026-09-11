#pragma once

enum class SafetyStateResult
{
    Updated,
    AlreadyActive,
    AlreadyClear
};

class SafetyState
{
public:
    [[nodiscard]] bool estop_active() const;
    [[nodiscard]] bool hardware_fault_active() const;
    [[nodiscard]] bool safe() const;

    SafetyStateResult report_estop();
    SafetyStateResult report_hardware_fault();

    [[nodiscard]] SafetyStateResult clear_estop();
    [[nodiscard]] SafetyStateResult clear_hardware_fault();

private:
    bool estop_active_ = false;
    bool hardware_fault_active_ = false;
};