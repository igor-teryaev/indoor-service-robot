#include <iostream>

#include "control_state.h"
#include "control_authority.h"

int main()
{
    ControlState state;

    const auto result = state.request_autonomous_control();

    std::cout
        << "Request result: "
        << control_request_result_to_string(result)
        << std::endl;

    return 0;
}