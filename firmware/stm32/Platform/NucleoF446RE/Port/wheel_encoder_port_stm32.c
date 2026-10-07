#include "wheel_encoder_port.h"

#include "tim.h"

bool wheel_encoder_port_init(void)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_SET_COUNTER(&htim4, 0U);

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
    {
        return false;
    }

    if (HAL_TIM_Base_Start(&htim4) != HAL_OK)
    {
        (void)HAL_TIM_Base_Stop(&htim3);
        return false;
    }

    return true;
}

WheelEncoderCounts wheel_encoder_port_read(void)
{
    const WheelEncoderCounts counts =
    {
        .left = __HAL_TIM_GET_COUNTER(&htim3),
        .right = __HAL_TIM_GET_COUNTER(&htim4),
    };

    return counts;
}