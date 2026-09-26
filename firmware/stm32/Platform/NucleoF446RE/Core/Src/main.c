/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motion_command_guard.h"
#include "uart_protocol_receiver.h"
#include "uart_rx_port.h"
#include "uart_link_manager.h"
#include "stm32_motion_protocol_manager.h"
#include "protocol_message_type.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MOTION_COMMAND_TIMEOUT_MS 250U
#define LINK_HEARTBEAT_TIMEOUT_MS 1000U
/* ARC101 open-loop calibration; this is not closed-loop velocity control. */
#define ARC101_OPEN_LOOP_FULL_SCALE_MM_S 400U
#define ARC101_MINIMUM_START_COMMAND 800U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static UartRxQueue uart_rx_queue;
static UartProtocolReceiver uart_protocol_receiver;
static Stm32MotionProtocolManager motion_protocol_manager;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void reset_link_and_stop_motion(void)
{
  (void)uart_link_manager_init(
      LINK_HEARTBEAT_TIMEOUT_MS);

  stm32_motion_protocol_manager_reset(
      &motion_protocol_manager);

  (void)motion_command_guard_stop();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM8_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  if (!motion_command_guard_init(
          MOTION_COMMAND_TIMEOUT_MS))
  {
    Error_Handler();
  }

  const WheelVelocityFeedforwardConfig feedforward_config =
  {
    .max_velocity_mm_s =
        ARC101_OPEN_LOOP_FULL_SCALE_MM_S,

    .minimum_start_command =
        ARC101_MINIMUM_START_COMMAND
  };

  if (!stm32_motion_protocol_manager_init(
          &motion_protocol_manager,
          &feedforward_config))
  {
    Error_Handler();
  }

  if (!uart_link_manager_init(
        LINK_HEARTBEAT_TIMEOUT_MS))
  {
    Error_Handler();
  }

  if (!uart_protocol_receiver_init(
        &uart_protocol_receiver,
        &uart_rx_queue))
  {
    Error_Handler();
  }

  if (!uart_rx_port_init(&uart_rx_queue))
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    const uint32_t now_ms = HAL_GetTick();

    const Stm32MotionProtocolManagerResult motion_update =
        stm32_motion_protocol_manager_update(
            &motion_protocol_manager,
            now_ms);

    if ((motion_update ==
         STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED) ||
        (motion_update ==
         STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED))
    {
      reset_link_and_stop_motion();
    }

    const ProtocolFrame *frame = NULL;

    const UartProtocolReceiverResult receive_result =
        uart_protocol_receiver_poll(
            &uart_protocol_receiver,
            &frame);

    if ((receive_result == UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW) ||
        (receive_result == UART_PROTOCOL_RECEIVER_RESULT_INVALID_ARGUMENT))
    {
      reset_link_and_stop_motion();
    }
    else if (receive_result == UART_PROTOCOL_RECEIVER_RESULT_FRAME)
    {
      switch (frame->message_type)
      {
      case PROTOCOL_MESSAGE_TYPE_LINK_SYNC:
      case PROTOCOL_MESSAGE_TYPE_HEARTBEAT:
        {
          const UartLinkManagerResult link_result =
              uart_link_manager_handle(
                  frame,
                  now_ms);

          /*
           * A successful LINK_SYNC starts a new link epoch.
           *
           * Any loss or rejection of synchronization also
           * invalidates the current motion session.
           */
          if ((link_result ==
               UART_LINK_MANAGER_RESULT_SYNCHRONIZED) ||
              !uart_link_manager_is_synchronized())
          {
            stm32_motion_protocol_manager_reset(
                &motion_protocol_manager);
          }

          break;
        }

      case PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND:
      case PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY:
        {
          const Stm32MotionProtocolManagerResult motion_result =
              stm32_motion_protocol_manager_handle(
                  &motion_protocol_manager,
                  frame,
                  uart_link_manager_is_synchronized(),
                  now_ms);

          if ((motion_result ==
               STM32_MOTION_PROTOCOL_MANAGER_RESULT_APPLY_FAILED) ||
              (motion_result ==
               STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED) ||
              (motion_result ==
               STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED))
          {
            reset_link_and_stop_motion();
          }

          break;
        }

      default:
        break;
      }
    }

    const UartLinkManagerUpdate link_update = uart_link_manager_update(now_ms);

    if ((link_update == UART_LINK_MANAGER_UPDATE_LINK_LOST) ||
        (link_update == UART_LINK_MANAGER_UPDATE_STOP_FAILED))
    {
      stm32_motion_protocol_manager_reset(&motion_protocol_manager);
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
