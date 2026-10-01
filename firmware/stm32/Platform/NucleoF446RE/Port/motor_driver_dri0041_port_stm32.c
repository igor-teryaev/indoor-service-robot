#include "motor_driver_dri0041_port.h"
#include "motor_driver.h"

#include "main.h"
#include "tim.h"

static bool port_initialized = false;

static uint32_t compare_from_duty(uint16_t duty)
{
    return ((uint32_t)duty * (htim8.Init.Period + 1U)) /
        (uint32_t)MOTOR_DRIVER_COMMAND_MAX;
}

static void set_direction_pins(
    const Dri0041PortControl *control)
{
    HAL_GPIO_WritePin(
        DRI0041_LEFT_IN1_GPIO_Port,
        DRI0041_LEFT_IN1_Pin,
        control->left_in1
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(
        DRI0041_LEFT_IN2_GPIO_Port,
        DRI0041_LEFT_IN2_Pin,
        control->left_in2
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(
        DRI0041_RIGHT_IN3_GPIO_Port,
        DRI0041_RIGHT_IN3_Pin,
        control->right_in3
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(
        DRI0041_RIGHT_IN4_GPIO_Port,
        DRI0041_RIGHT_IN4_Pin,
        control->right_in4
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET);
}

bool motor_driver_dri0041_port_init(void)
{
    port_initialized = false;

    const Dri0041PortControl brake_control = {0};
    set_direction_pins(&brake_control);

    if (htim8.Instance != TIM8)
    {
        return false;
    }

    __HAL_TIM_SET_COMPARE(
        &htim8,
        TIM_CHANNEL_1,
        0U);

    __HAL_TIM_SET_COMPARE(
        &htim8,
        TIM_CHANNEL_3,
        0U);

    if (HAL_TIM_PWM_Start(
            &htim8,
            TIM_CHANNEL_1) != HAL_OK)
    {
        return false;
    }

    if (HAL_TIM_PWM_Start(
            &htim8,
            TIM_CHANNEL_3) != HAL_OK)
    {
        (void)HAL_TIM_PWM_Stop(
            &htim8,
            TIM_CHANNEL_1);

        return false;
    }

    port_initialized = true;
    return true;
}

bool motor_driver_dri0041_port_apply(
    const Dri0041PortControl *control)
{
    if ((control == NULL) ||
        (!port_initialized) ||
        (htim8.Instance != TIM8))
    {
        return false;
    }

    if ((control->left_duty >
         MOTOR_DRIVER_COMMAND_MAX) ||
        (control->right_duty >
         MOTOR_DRIVER_COMMAND_MAX))
    {
        return false;
    }

    const uint32_t left_compare =
        compare_from_duty(
            control->left_duty);

    const uint32_t right_compare =
        compare_from_duty(
            control->right_duty);

    /*
     * Direction changes are serialized by the DRI0041 driver.
     * A reversal reaches this port only after the required brake interval.
     */
    set_direction_pins(control);

    __HAL_TIM_SET_COMPARE(
        &htim8,
        TIM_CHANNEL_1,
        left_compare);

    __HAL_TIM_SET_COMPARE(
        &htim8,
        TIM_CHANNEL_3,
        right_compare);

    return true;
}

uint32_t motor_driver_dri0041_port_now_ms(void)
{
    return HAL_GetTick();
}