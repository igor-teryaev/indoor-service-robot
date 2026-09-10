#include "safety_state.h"

SafetyStateResult SafetyState::clear_hardware_fault()
{
    if (!hardware_fault_active_)
        return SafetyStateResult::AlreadyClear;

    hardware_fault_active_ = false;
    return SafetyStateResult::Updated;
}

SafetyStateResult SafetyState::clear_estop()
{
    if (!estop_active_)
        return SafetyStateResult::AlreadyClear;

    estop_active_ = false;
    return SafetyStateResult::Updated;
}

SafetyStateResult SafetyState::report_estop()
{
    if (estop_active_)
        return SafetyStateResult::AlreadyActive;

    estop_active_ = true;
    return SafetyStateResult::Updated;
}

SafetyStateResult SafetyState::report_hardware_fault()
{
    if (hardware_fault_active_)
        return SafetyStateResult::AlreadyActive;

    hardware_fault_active_ = true;
    return SafetyStateResult::Updated;
}

bool SafetyState::estop_active() const
{
    return estop_active_;
}

bool SafetyState::hardware_fault_active() const
{
    return hardware_fault_active_;
}

bool SafetyState::safe() const
{
    return !(estop_active_ || hardware_fault_active_);
}
