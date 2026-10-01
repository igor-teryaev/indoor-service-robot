#include "motor_driver_drv8833_port.h"
#include "motor_driver.h"

#include "main.h"
#include "tim.h"

static bool port_initialized = false;

static uint32_t compare_from_duty(uint16_t duty)
{
    return ((uint32_t)duty * (htim8.Init.Period + 1U)) /
        (uint32_t)MOTOR_DRIVER_COMMAND_MAX;
}

bool motor_driver_drv8833_port_init(void)
{
    port_initialized = false;

    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_RESET);

    if (htim8.Instance != TIM8)
    {
        return false;
    }

    // 1. Встановлюємо Compare (ШІМ) усіх чотирьох каналів у 0U
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);

    // 2. Почергово запускаємо ШІМ на каналах 1–4 з перевіркою статусу HAL
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK) goto fail;
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK) goto fail;
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK) goto fail;
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK) goto fail;
    port_initialized = true;

    return true;

    fail:
        HAL_GPIO_WritePin(
            MOTOR_STBY_GPIO_Port,
            MOTOR_STBY_Pin,
            GPIO_PIN_RESET);

        (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);
        (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);
        (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_3);
        (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);

        return false;
}

bool motor_driver_drv8833_port_apply(const Drv8833PortControl *control)
{
    if ((control == NULL) ||
        (!port_initialized) ||
        (htim8.Instance != TIM8))
    {
        return false;
    }

    const uint16_t ain1_duty = control->ain1_duty;
    const uint16_t ain2_duty = control->ain2_duty;
    const uint16_t bin1_duty = control->bin1_duty;
    const uint16_t bin2_duty = control->bin2_duty;

    if ((ain1_duty > MOTOR_DRIVER_COMMAND_MAX) ||
        (ain2_duty > MOTOR_DRIVER_COMMAND_MAX) ||
        (bin1_duty > MOTOR_DRIVER_COMMAND_MAX) ||
        (bin2_duty > MOTOR_DRIVER_COMMAND_MAX))
    {
        return false;
    }

    const uint32_t ccr1 = compare_from_duty(ain1_duty);
    const uint32_t ccr2 = compare_from_duty(ain2_duty);
    const uint32_t ccr3 = compare_from_duty(bin1_duty);
    const uint32_t ccr4 = compare_from_duty(bin2_duty);

    SET_BIT(htim8.Instance->CR1, TIM_CR1_UDIS);

    htim8.Instance->CCR1 = ccr1;
    htim8.Instance->CCR2 = ccr2;
    htim8.Instance->CCR3 = ccr3;
    htim8.Instance->CCR4 = ccr4;

    CLEAR_BIT(htim8.Instance->CR1, TIM_CR1_UDIS);

    HAL_GPIO_WritePin(
        MOTOR_STBY_GPIO_Port,
        MOTOR_STBY_Pin,
        GPIO_PIN_SET);

    return true;
}