/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  @verbatim
  ==============================================================================
                    ##### IMPORTANT NOTE #####
  ==============================================================================

  This application requests having the stm32wb5x_Mac_802_15_4_fw.bin binary
  flashed on the Wireless Coprocessor.
  If it is not the case, you need to use STM32CubeProgrammer to load the appropriate
  binary.

  All available binaries are located under following directory:
  /Projects/STM32_Copro_Wireless_Binaries

  Refer to UM2237 to learn how to use/install STM32CubeProgrammer.
  Refer to /Projects/STM32_Copro_Wireless_Binaries/ReleaseNote.html for the
  detailed procedure to change the Wireless Coprocessor binary.

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  *
  ******************************************************************************
 */


/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private function definition -------------------------------------------------*/

static void SystemClock_Config( void );
static void SystemPower_Config(void);
static void Reset_Device( void );
static void Reset_IPCC( void );
static void Reset_BackupDomain( void );
static void Init_Debug( void );

/**
  * @brief  Main function
  *
  * @param  None
  * @retval 0
  */

int main(void)
{

  /* Initialize the HAL */
  HAL_Init();


  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED3);

  /* Reset HW IP IPCC/Backup Domain */
  Reset_Device();

  /*Configure the system clock */
  SystemClock_Config();

  /* Configure the system Power Mode */
  SystemPower_Config();

  Init_Debug();

  APP_DBG("**** FFD MAC 802.15.4 EXAMPLE ****");

  /* Application init */
  APP_ENTRY_Init(APPE_FULL);

  /* Main Loop  */
  while (1)
  {

    SCH_Run(~0);
  }
}

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/

/**
 * @brief  Reset Device
 *
 * @note
 *
 * @param  None
 * @retval None
 */
static void Reset_Device( void )
{
#if ( CFG_HW_RESET_BY_FW == 1 )
  Reset_BackupDomain();
  Reset_IPCC();
#endif

  return;
}

/**
 * @brief  Reset IPCC
 *
 * @note
 *
 * @param  None
 * @retval None
 */
static void Reset_IPCC( void )
{
  LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);

  LL_C1_IPCC_ClearFlag_CHx(
                           IPCC,
                           LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                               | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_ClearFlag_CHx(
                           IPCC,
                           LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                               | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C1_IPCC_DisableTransmitChannel(
                                    IPCC,
                                    LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                                        | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_DisableTransmitChannel(
                                    IPCC,
                                    LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                                        | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C1_IPCC_DisableReceiveChannel(
                                   IPCC,
                                   LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                                       | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  LL_C2_IPCC_DisableReceiveChannel(
                                   IPCC,
                                   LL_IPCC_CHANNEL_1 | LL_IPCC_CHANNEL_2 | LL_IPCC_CHANNEL_3 | LL_IPCC_CHANNEL_4
                                       | LL_IPCC_CHANNEL_5 | LL_IPCC_CHANNEL_6);

  return;
}

/**
 * @brief  Reset the backup domain
 *
 * @note
 *
 * @param  None
 * @retval None
 */
static void Reset_BackupDomain( void )
{
  if ((LL_RCC_IsActiveFlag_PINRST() != FALSE) && (LL_RCC_IsActiveFlag_SFTRST() == FALSE))
  {
    HAL_PWR_EnableBkUpAccess(); /**< Enable access to the RTC registers */

    /**
     *  Write twice the value to flush the APB-AHB bridge
     *  This bit shall be written in the register before writing the next one
     */
    HAL_PWR_EnableBkUpAccess();

    __HAL_RCC_BACKUPRESET_FORCE();
    __HAL_RCC_BACKUPRESET_RELEASE();
  }

  return;
}

/**
 * @brief  Configure the system clock
 *
 * @note   This API configures
 *          - The system clock source
 *          - The AHBCLK, APBCLK dividers
 *          - The flash latency
 *          - The PLL settings (when required)
 *
 * @param  None
 * @retval None
 */
static void SystemClock_Config( void )
{
  /**
   *  Write twice the value to flush the APB-AHB bridge to ensure the  bit is written
   */
  HAL_PWR_EnableBkUpAccess(); /**< Enable access to the RTC registers */
  HAL_PWR_EnableBkUpAccess();

  /**
   * Select LSE clock
   */
  LL_RCC_LSE_Enable();
  while(!LL_RCC_LSE_IsReady());

  /**
   * Select wakeup source of BLE RF
   */
  LL_RCC_SetRFWKPClockSource(LL_RCC_RFWKP_CLKSOURCE_LSE);

  /**
   * Switch OFF LSI
   */
  LL_RCC_LSI1_Disable();

  /**
   * Set RNG on HSI48
   */
  LL_RCC_HSI48_Enable();
  while(!LL_RCC_HSI48_IsReady());
  LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);

  return;
}

/**
 * @brief  Configure the system for power optimization
 *
 * @note  This API configures the system to be ready for low power mode
 *
 * @param  None
 * @retval None
 */
static void SystemPower_Config( void )
{
  LPM_Conf_t LowPowerModeConfiguration;

  /**
   * Select HSI as system clock source after Wake Up from Stop mode
   */
  LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);

  LowPowerModeConfiguration.Stop_Mode_Config = LPM_StopMode2;
  LowPowerModeConfiguration.OFF_Mode_Config = LPM_Standby;
  LPM_SetConf(&LowPowerModeConfiguration);

  return;
}

/**
 * @brief  Initializes the system for debug purpose
 *
 * @note
 *
 * @param  None
 * @retval None
 */
static void Init_Debug( void )
{
#if (CFG_DEBUGGER_SUPPORTED == 1)
  /**
   * Keep debugger enabled while in any low power mode
   */
  HAL_DBGMCU_EnableDBGSleepMode();
  /* HAL_DBGMCU_EnableDBGStopMode(); */
  /* HAL_DBGMCU_EnableDBGStandbyMode(); */

  /***************** ENABLE DEBUGGER *************************************/
  LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_48);
  LL_C2_EXTI_EnableIT_32_63(LL_EXTI_LINE_48);

#else

  GPIO_InitTypeDef gpio_config = {0};

  gpio_config.Pull = GPIO_NOPULL;
  gpio_config.Mode = GPIO_MODE_ANALOG;

  gpio_config.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13;
  __HAL_RCC_GPIOA_CLK_ENABLE();
  HAL_GPIO_Init(GPIOA, &gpio_config);
  __HAL_RCC_GPIOA_CLK_DISABLE();

  gpio_config.Pin = GPIO_PIN_4 | GPIO_PIN_3;
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_Init(GPIOB, &gpio_config);
  __HAL_RCC_GPIOB_CLK_DISABLE();

  HAL_DBGMCU_DisableDBGSleepMode();
  HAL_DBGMCU_DisableDBGStopMode();
  HAL_DBGMCU_DisableDBGStandbyMode();

#endif /* (CFG_DEBUGGER_SUPPORTED == 1) */

#if(CFG_DEBUG_TRACE != 0)
  DbgTraceInit();
#endif

  return;
}

/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
