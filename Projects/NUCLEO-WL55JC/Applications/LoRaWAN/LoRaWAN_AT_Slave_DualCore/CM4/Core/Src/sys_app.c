/**
  ******************************************************************************
  * @file    sys_app.c
  * @author  MCD Application Team
  * @brief   Initializes HW and SW system entities (not related to the radio)
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "platform.h"
#include "sys_app.h"
#include "adc_if.h"
#include "stm32_seq.h"
#include "stm32_systime.h"
#include "stm32_lpm.h"
#include "utilities_def.h"
#include "sys_debug.h"
#include "timer_if.h"
#include "msg_id.h"
#include "mbmuxif_sys.h"
#include "mbmuxif_trace.h"
#include "mbmuxif_lora.h"
#include "mbmuxif_radio.h"
/*#include "mbmuxif_kms.h"*/

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
#define MAX_TS_SIZE (int) 16

/**
  * Defines the maximum battery level
  */
#define LORAWAN_MAX_BAT   254
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
uint32_t InstanceIndex;
uint8_t SYS_Cm0plusRdyNotificationFlag = 0;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MBMUXIF_Init(void);

/**
  * @brief  Set all pins such to minimized consumption (necessary for some STM32 families)
  * @param none
  * @retval None
  */
static void Gpio_PreInit(void);
/**
  * @brief Returns sec and msec based on the systime in use
  * @param none
  * @retval  none
  */
static void TimestampNow(uint8_t *buff, uint16_t *size);

/**
  * @brief  it calls UTIL_ADV_TRACE_VSNPRINTF
  */
static void tiny_snprintf_like(char *buf, uint32_t maxsize, const char *strFormat, ...);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */
/* Exported functions ---------------------------------------------------------*/
/**
  * @brief initialises the system (dbg pins, trace, mbmux, systiemr, LPM, ...)
  * @param none
  * @retval  none
  */
void SystemApp_Init(void)
{
  /* USER CODE BEGIN SystemApp_Init_1 */

  /* USER CODE END SystemApp_Init_1 */

  /* Ensure that MSI is wake-up system clock */
  __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);

  Gpio_PreInit();

  /* Configure the debug mode*/
  DBG_Init();

  /*Initialize the terminal */
  UTIL_ADV_TRACE_Init();
  UTIL_ADV_TRACE_RegisterTimeStampFunction(TimestampNow);

  /*Set verbose LEVEL*/
  UTIL_ADV_TRACE_SetVerboseLevel(VERBOSE_LEVEL);
  /*Initialize the temperature and Battery measurement services */
  SYS_InitMeasurement();

  /*Init low power manager*/
  UTIL_LPM_Init();
  /* Disable Stand-by mode */
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);

#if defined (LOW_POWER_DISABLE) && (LOW_POWER_DISABLE == 1)
  /* Disable Stop Mode */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_Id), UTIL_LPM_DISABLE);
#elif !defined (LOW_POWER_DISABLE)
#error LOW_POWER_DISABLE not defined
#endif /* LOW_POWER_DISABLE */

  /*Initialize MBMux (to be done after LPM because MBMux uses the sequencer) */
  MBMUXIF_Init();

  UTIL_TIMER_Init();

  /* USER CODE BEGIN SystemApp_Init_2 */

  /* USER CODE END SystemApp_Init_2 */
}

/**
  * @brief  Process System Notifications
  */
void Process_Sys_Notif(MBMUX_ComParam_t *ComObj)
{
  /* USER CODE BEGIN Process_Sys_Notif_1 */

  /* USER CODE END Process_Sys_Notif_1 */
  uint32_t  notif_ack_id;

  notif_ack_id = ComObj->MsgId;

  switch (notif_ack_id)
  {
    case SYS_OTHER_MSG_ID:
      APP_LOG(TS_ON, VLEVEL_H, "CM4<(System)\r\n");
      /* prepare ack buffer*/
      ComObj->ParamCnt = 0;
      ComObj->ReturnVal = 7; /* dummy value for test */
      break;

    default:
      break;
  }

  /* Send ack*/
  APP_LOG(TS_ON, VLEVEL_H, "CM4>(System)\r\n");
  MBMUXIF_SystemSendAck(FEAT_INFO_SYSTEM_ID);
  /* USER CODE BEGIN Process_Sys_Notif_2 */

  /* USER CODE END Process_Sys_Notif_2 */
}

void UTIL_SEQ_EvtIdle(uint32_t task_id_bm, uint32_t evt_waited_bm)
{
  /**
    * overwrites the __weak UTIL_SEQ_EvtIdle() in stm32_seq.c
    * all to process all tack except task_id_bm
    */
  /* USER CODE BEGIN UTIL_SEQ_EvtIdle_1 */

  /* USER CODE END UTIL_SEQ_EvtIdle_1 */
  UTIL_SEQ_Run(~task_id_bm);
  /* USER CODE BEGIN UTIL_SEQ_EvtIdle_2 */

  /* USER CODE END UTIL_SEQ_EvtIdle_2 */
  return;
}

/**
  * @brief redefines __weak function in stm32_seq.c such to enter low power
  * @param none
  * @retval  none
  */
void UTIL_SEQ_Idle(void)
{
  /* USER CODE BEGIN UTIL_SEQ_Idle_1 */

  /* USER CODE END UTIL_SEQ_Idle_1 */
  UTIL_LPM_EnterLowPower();
  /* USER CODE BEGIN UTIL_SEQ_Idle_2 */

  /* USER CODE END UTIL_SEQ_Idle_2 */
}

uint8_t GetBatteryLevel(void)
{
  uint8_t batteryLevel = 0;
  uint16_t batteryLevelmV;

  /* USER CODE BEGIN GetBatteryLevel_0 */

  /* USER CODE END GetBatteryLevel_0 */

  batteryLevelmV = (uint16_t) SYS_GetBatteryLevel();

  /* Convert batterey level from mV to linea scale: 1 (very low) to 254 (fully charged) */
  if (batteryLevelmV > VDD_BAT)
  {
    batteryLevel = LORAWAN_MAX_BAT;
  }
  else if (batteryLevelmV < VDD_MIN)
  {
    batteryLevel = 0;
  }
  else
  {
    batteryLevel = (((uint32_t)(batteryLevelmV - VDD_MIN) * LORAWAN_MAX_BAT) / (VDD_BAT - VDD_MIN));
  }

  APP_LOG(TS_ON, VLEVEL_M, "VDDA= %d\r\n", batteryLevel);

  /* USER CODE BEGIN GetBatteryLevel_2 */

  /* USER CODE END GetBatteryLevel_2 */

  return batteryLevel;  /* 1 (very low) to 254 (fully charged) */
}

uint16_t GetTemperatureLevel(void)
{
  uint16_t temperatureLevel = 0;

  temperatureLevel = (uint16_t)(SYS_GetTemperatureLevel() / 256);
  /* USER CODE BEGIN GetTemperatureLevel */

  /* USER CODE END GetTemperatureLevel */
  return temperatureLevel;
}

/* USER CODE BEGIN ExF */

/* USER CODE END ExF */

/* Private functions ---------------------------------------------------------*/

/**
  * @brief Test: Initialize MBMUX, wait CM0PLUS is ready, gets CM0PLUS capabilities, Initialises other features
  * @param none
  * @retval none
  */
static void MBMUXIF_Init(void)
{
  /* USER CODE BEGIN MBMUXIF_Init_1 */

  /* USER CODE END MBMUXIF_Init_1 */
  FEAT_INFO_List_t *p_cm0plus_supprted_features_list;
  int8_t init_status;

  APP_LOG(TS_ON, VLEVEL_H, "\r\nCM4: System Initialisation started \r\n");

  init_status = MBMUXIF_SystemInit();
  if (init_status < 0)
  {
    while (1) {}
  }

  /* start CM0PLUS */
  /* Note: when debugging in order to connect with the deggugger CPUU2 shall be start using workspace CM4 starts CM0PLUS */
  /* On the other hand is up to the devepoper make sure the CM0PLUS debugger is run after CM4 debugger */
  HAL_PWREx_ReleaseCore(PWR_CORE_CPU2);
  /* CM4 has started and it has reset the mailbox and initialised the MbMux; */
  /* once CM0PLUS is also initialised it send a SYS notification */
  MBMUXIF_SetCpusSynchroFlag(CPUS_BOOT_SYNC_ALLOW_CPU2_TO_START);

  APP_LOG(TS_ON, VLEVEL_H, "CM4: System Initialisation done: Wait for CM0PLUS \r\n");

  MBMUXIF_WaitCm0MbmuxIsInitialised();

  APP_LOG(TS_ON, VLEVEL_H, "CM0PLUS: System Initialisation started \r\n");

  p_cm0plus_supprted_features_list = MBMUXIF_SystemSendCm0plusInfoListReq();
  MBMUX_SetCm0plusFeatureListPtr(p_cm0plus_supprted_features_list);

  APP_LOG(TS_ON, VLEVEL_H, "System Initialisation CM4-CM0PLUS completed \r\n");

  init_status = MBMUXIF_SystemPrio_Add(FEAT_INFO_SYSTEM_NOTIF_PRIO_A_ID);
  if (init_status < 0)
  {
    Error_Handler();
  }
  MBMUXIF_SetCpusSynchroFlag(CPUS_BOOT_SYNC_RTC_REGISTERED);
  APP_LOG(TS_ON, VLEVEL_H, "System_Priority_A Registration for RTC Alarm handling completed \r\n");

  init_status = MBMUXIF_TraceInit();
  if (init_status < 0)
  {
    Error_Handler();
  }
  APP_LOG(TS_ON, VLEVEL_H, "Trace registration CM4-CM0PLUS completed \r\n");

  init_status = MBMUXIF_LoraInit();
  if (init_status < 0)
  {
    Error_Handler();
  }
  APP_LOG(TS_ON, VLEVEL_H, "Radio registration CM4-CM0PLUS completed \r\n");

  init_status = MBMUXIF_RadioInit();
  if (init_status < 0)
  {
    Error_Handler();
  }
  APP_LOG(TS_ON, VLEVEL_H, "Radio registration CM4-CM0PLUS completed \r\n");

  /* USER CODE BEGIN MBMUXIF_Init_Last */

  /* USER CODE END MBMUXIF_Init_Last */
}

static void TimestampNow(uint8_t *buff, uint16_t *size)
{
  /* USER CODE BEGIN TimestampNow_1 */

  /* USER CODE END TimestampNow_1 */
  SysTime_t curtime = SysTimeGet();
  tiny_snprintf_like((char *)buff, MAX_TS_SIZE, "%ds%03d:", curtime.Seconds, curtime.SubSeconds);
  *size = strlen((char *)buff);
  /* USER CODE BEGIN TimestampNow_2 */

  /* USER CODE END TimestampNow_2 */
}

static void Gpio_PreInit(void)
{
  /* USER CODE BEGIN Gpio_PreInit_1 */

  /* USER CODE END Gpio_PreInit_1 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Configure all IOs in analog input              */
  /* Except PA143 and PA14 (SWCLK and SWD) for debug*/
  /* PA13 and PA14 are configured in debug_init     */
  /* Configure all GPIO as analog to reduce current consumption on non used IOs */
  /* Enable GPIOs clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  /* All GPIOs except debug pins (SWCLK and SWD) */
  GPIO_InitStruct.Pin = GPIO_PIN_All & (~(GPIO_PIN_13 | GPIO_PIN_14));
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* All GPIOs */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /* Disable GPIOs clock */
  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
  /* USER CODE BEGIN Gpio_PreInit_2 */

  /* USER CODE END Gpio_PreInit_2 */
}

/* Disable StopMode when traces need to be printed */
void UTIL_ADV_TRACE_PreSendHook(void)
{
  /* USER CODE BEGIN UTIL_ADV_TRACE_PreSendHook_1 */

  /* USER CODE END UTIL_ADV_TRACE_PreSendHook_1 */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_UART_TX_Id), UTIL_LPM_DISABLE);
  /* USER CODE BEGIN UTIL_ADV_TRACE_PreSendHook_2 */

  /* USER CODE END UTIL_ADV_TRACE_PreSendHook_2 */
}
/* Re-enable StopMode when traces have been printed */
void UTIL_ADV_TRACE_PostSendHook(void)
{
  /* USER CODE BEGIN UTIL_LPM_SetStopMode_1 */

  /* USER CODE END UTIL_LPM_SetStopMode_1 */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_UART_TX_Id), UTIL_LPM_ENABLE);
  /* USER CODE BEGIN UTIL_LPM_SetStopMode_2 */

  /* USER CODE END UTIL_LPM_SetStopMode_2 */
}

static void tiny_snprintf_like(char *buf, uint32_t maxsize, const char *strFormat, ...)
{
  /* USER CODE BEGIN tiny_snprintf_like_1 */

  /* USER CODE END tiny_snprintf_like_1 */
  va_list vaArgs;
  va_start(vaArgs, strFormat);
  UTIL_ADV_TRACE_VSNPRINTF(buf, maxsize, strFormat, vaArgs);
  va_end(vaArgs);
  /* USER CODE BEGIN tiny_snprintf_like_2 */

  /* USER CODE END tiny_snprintf_like_2 */
}

/* USER CODE BEGIN PrFD */

/* USER CODE END PrFD */
/* HAL overload functions ---------------------------------------------------------*/

/**
  * @brief This function configures the source of the time base.
  * @brief  don't enable systick
  * @param TickPriority: Tick interrupt priority.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  /*Don't enable SysTick if TIMER_IF is based on other counters (e.g. RTC) */
  /* USER CODE BEGIN HAL_InitTick_1 */

  /* USER CODE END HAL_InitTick_1 */
  return HAL_OK;
  /* USER CODE BEGIN HAL_InitTick_2 */

  /* USER CODE END HAL_InitTick_2 */
}

/**
  * @brief Provide a tick value in millisecond measured using RTC
  * @note This function overwrites the __weak one from HAL
  * @retval tick value
  */
uint32_t HAL_GetTick(void)
{
  /* TIMER_IF can be based onother counter the SysTick e.g. RTC */
  /* USER CODE BEGIN HAL_GetTick_1 */

  /* USER CODE END HAL_GetTick_1 */
  return TIMER_IF_GetTimerValue();
  /* USER CODE BEGIN HAL_GetTick_2 */

  /* USER CODE END HAL_GetTick_2 */
}

/**
  * @brief This function provides delay (in ms)
  * @param Delay: specifies the delay time length, in milliseconds.
  * @retval None
  */
void HAL_Delay(__IO uint32_t Delay)
{
  /* TIMER_IF can be based onother counter the SysTick e.g. RTC */
  /* USER CODE BEGIN HAL_Delay_1 */

  /* USER CODE END HAL_Delay_1 */
  TIMER_IF_DelayMs(Delay);
  /* USER CODE BEGIN HAL_Delay_2 */

  /* USER CODE END HAL_Delay_2 */
}

/* USER CODE BEGIN Overload_HAL_weaks */

/* USER CODE END Overload_HAL_weaks */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

