/**
 ******************************************************************************
 * @file    app_entry.c
 * @author  MCD Application Team
 * @brief   Entry point of the Application
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
 ******************************************************************************
 */


/* Includes ------------------------------------------------------------------*/
#include "app_common.h"
#include "main.h"
#include "app_entry.h"
#include "app_thread.h"
#include "app_conf.h"
#include "hw_conf.h"
#include "lpm.h"
#include "scheduler.h"
#include "stm_logging.h"
#include "dbg_trace.h"
#include "shci_tl.h"
#include "shci.h"

/* Private includes -----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
/* POOL_SIZE = 2(TL_PacketHeader_t) + 258 (3(TL_EVT_HDR_SIZE) + 255(Payload size)) */
#define POOL_SIZE (CFG_TL_EVT_QUEUE_LENGTH * 4U * DIVC(( sizeof(TL_PacketHeader_t) + TL_EVENT_FRAME_SIZE ), 4U))
/* USER CODE BEGIN PD */

/* USER CODE END PD */
/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */
/* Private variables -------------------------------------------------*/
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t EvtPool[POOL_SIZE];
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static TL_CmdPacket_t SystemCmdBuffer;
PLACE_IN_SECTION("MB_MEM2") ALIGN(4) static uint8_t SystemSpareEvtBuffer[sizeof(TL_PacketHeader_t) + TL_EVT_HDR_SIZE + 255U];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Global function prototypes -----------------------------------------------*/
size_t DbgTraceWrite(int handle, const unsigned char * buf, size_t bufSize);

/* Private function prototypes -----------------------------------------------*/
static void SystemPower_Config( void );
static void Init_Debug( void );
static void Switch_On_HSI( void );
static void APPE_SysStatusNot( SHCI_TL_CmdStatus_t status );
static void APPE_SysUserEvtRx( void * pPayload );
static void APPE_SysEvtReadyProcessing( void );
static void APPE_SysEvtError( SCHI_SystemErrCode_t ErrorCode);
static void appe_Tl_Init( void );
/* USER CODE BEGIN PFP */
static void Led_Init( void );
static void Button_Init( void );
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void APPE_Init( void )
{
  SystemPower_Config(); /**< Configure the system Power Mode */
  
  HW_TS_Init(hw_ts_InitMode_Full, &hrtc); /**< Initialize the TimerServer */

  Init_Debug();
/* USER CODE BEGIN APPE_Init_1 */
  /**
   * The Standby mode should not be entered before the initialization is over
   * The default state of the Low Power Manager is to allow the Standby Mode so an request is needed here
   */
  LPM_SetOffMode(1 << CFG_LPM_APP, LPM_OffMode_Dis);
  Led_Init();
  Button_Init();
/* USER CODE END APPE_Init_1 */
  /**
   * The Standby mode should not be entered before the initialization is over
   * The default state of the Low Power Manager is to allow the Standby Mode so an request is needed here
   */
  appe_Tl_Init(); /* Initialize all transport layers */

  /**
   * From now, the application is waiting for the ready event ( VS_HCI_C2_Ready )
   * received on the system channel before starting the Stack
   * This system event is received with APPE_SysUserEvtRx()
   */
/* USER CODE BEGIN APPE_Init_2 */

/* USER CODE END APPE_Init_2 */
  return;
}
/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
static void Init_Debug( void )
{
#if (CFG_DEBUGGER_SUPPORTED == 1)
  /**
   * Keep debugger enabled while in any low power mode
   */
  HAL_DBGMCU_EnableDBGSleepMode();
  
  

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

  /**< Configure low power manager */
  LowPowerModeConfiguration.Stop_Mode_Config = LPM_StopMode2;
  LowPowerModeConfiguration.OFF_Mode_Config = LPM_Standby;
  LPM_SetConf(&LowPowerModeConfiguration);

#if (CFG_USB_INTERFACE_ENABLE != 0)
  /**
   *  Enable USB power
   */
  HAL_PWREx_EnableVddUSB();
#endif

  return;
}

static void appe_Tl_Init( void )
{
  TL_MM_Config_t tl_mm_config;
  SHCI_TL_HciInitConf_t SHci_Tl_Init_Conf;

  /**< Reference table initialization */
  TL_Init();

  /**< System channel initialization */
  SCH_RegTask( CFG_TASK_SYSTEM_HCI_ASYNCH_EVT, shci_user_evt_proc );
  SHci_Tl_Init_Conf.p_cmdbuffer = (uint8_t*)&SystemCmdBuffer;
  SHci_Tl_Init_Conf.StatusNotCallBack = APPE_SysStatusNot;
  shci_init(APPE_SysUserEvtRx, (void*) &SHci_Tl_Init_Conf);

  /**< Memory Manager channel initialization */
  tl_mm_config.p_BleSpareEvtBuffer = 0;
  tl_mm_config.p_SystemSpareEvtBuffer = SystemSpareEvtBuffer;
  tl_mm_config.p_AsynchEvtPool = EvtPool;
  tl_mm_config.AsynchEvtPoolSize = POOL_SIZE;
  TL_MM_Init( &tl_mm_config );

  TL_Enable();

  return;
}

static void Switch_On_HSI( void )
{
  LL_RCC_HSI_Enable();
  while(!LL_RCC_HSI_IsReady());
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI);

  return;
}

static void APPE_SysStatusNot( SHCI_TL_CmdStatus_t status )
{
  UNUSED(status);
  return;
}

/**
 * @brief Trap a notification coming from the M0 firmware
 * @param  pPayload  : payload associated to the notification
 *
 * @retval None
 */
static void APPE_SysUserEvtRx( void * pPayload )
{
  TL_AsynchEvt_t *p_sys_event;
  p_sys_event = (TL_AsynchEvt_t*)(((tSHCI_UserEvtRxParam*)pPayload)->pckt->evtserial.evt.payload);

  switch(p_sys_event->subevtcode)
  {
     case SHCI_SUB_EVT_CODE_READY:
         APPE_SysEvtReadyProcessing();
         break;
     case SHCI_SUB_EVT_ERROR_NOTIF:
         APPE_SysEvtError((SCHI_SystemErrCode_t) (p_sys_event->payload[0]));
         break;
     default:
         break;
  }
  return;
}

/**
 * @brief Notify a system error coming from the M0 firmware
 * @param  ErrorCode  : errorCode detected by the M0 firmware
 *
 * @retval None
 */
static void APPE_SysEvtError( SCHI_SystemErrCode_t ErrorCode)
{
  switch(ErrorCode)
  {
  case ERR_THREAD_LLD_FATAL_ERROR:
       APP_DBG("** ERR_THREAD : LLD_FATAL_ERROR \n");
       break;
  case ERR_THREAD_UNKNOWN_CMD:
       APP_DBG("** ERR_THREAD : UNKNOWN_CMD \n");
       break;
  default:
       APP_DBG("** ERR_THREAD : ErroCode=%d \n",ErrorCode);
       break;
  }
  return;
}

static void APPE_SysEvtReadyProcessing( void )
{
  /* Traces channel initialization */
  TL_TRACES_Init( );

  APP_THREAD_Init();
  LPM_SetOffMode(1U << CFG_LPM_APP, LPM_OffMode_En);
  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
static void Led_Init( void )
{
#if (CFG_LED_SUPPORTED == 1U)
  /**
   * Leds Initialization
   */

  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);

#endif

  return;
}

static void Button_Init( void )
{
#if (CFG_BUTTON_SUPPORTED == 1U)
  /**
   * Button Initialization
   */

  BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);
#endif

  return;
}


/* USER CODE END FD_LOCAL_FUNCTIONS */
/*************************************************************
 *
 * WRAP FUNCTIONS
 *
 *************************************************************/
 
void SCH_Idle( void )
{
#if ( CFG_LPM_SUPPORTED == 1)
  LPM_EnterModeSelected();
#endif
  return;
}

void LPM_EnterStopMode(void)
{
  /**
   * This function is called from CRITICAL SECTION
   */

  while( LL_HSEM_1StepLock( HSEM, CFG_HW_RCC_SEMID ) );

  if ( ! LL_HSEM_1StepLock( HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID ) )
  {
    if( LL_PWR_IsActiveFlag_C2DS() )
    {
      /* Release ENTRY_STOP_MODE semaphore */
      LL_HSEM_ReleaseLock( HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0 );

      Switch_On_HSI();
    }
  }
  else
  {
    Switch_On_HSI();
  }

  /* Release RCC semaphore */
  LL_HSEM_ReleaseLock( HSEM, CFG_HW_RCC_SEMID, 0 );

  return;
}

void LPM_ExitStopMode(void)
{
  /**
   * This function is called from CRITICAL SECTION
   */

  /* Release ENTRY_STOP_MODE semaphore */
  LL_HSEM_ReleaseLock( HSEM, CFG_HW_ENTRY_STOP_MODE_SEMID, 0 );

  if( (LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_HSI) || (LL_PWR_IsActiveFlag_C1STOP() != 0) )
  {
    LL_PWR_ClearFlag_C1STOP_C1STB();

    while( LL_HSEM_1StepLock( HSEM, CFG_HW_RCC_SEMID ) );

    if(LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
    {
      LL_RCC_HSE_Enable();
      while(!LL_RCC_HSE_IsReady());
      LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSE);
      while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSE);
    }
    else
    {
      /**
       * As long as the current application is fine with HSE as system clock source,
       * there is nothing to do here
       */
    }

    /* Release RCC semaphore */
    LL_HSEM_ReleaseLock( HSEM, CFG_HW_RCC_SEMID, 0 );
  }

  return;
}

/**
  * @brief  This function is called by the scheduler each time an event
  *         is pending.
  *
  * @param  evt_waited_bm : Event pending.
  * @retval None
  */
void SCH_EvtIdle( uint32_t evt_waited_bm )
{
  switch(evt_waited_bm)
  {
  case EVENT_ACK_FROM_M0_EVT:
    /* Does not allow other tasks when waiting for OT Cmd response */
    SCH_Run(0);
    break;
  case EVENT_SYNCHRO_BYPASS_IDLE:
    SCH_SetEvt(EVENT_SYNCHRO_BYPASS_IDLE);
    /* Run only the task CFG_TASK_MSG_FROM_M0_TO_M4 */
    SCH_Run(TASK_MSG_FROM_M0_TO_M4);
    break;
  default :
    /* default case */
    SCH_Run(~0);
    break;
  }
}

void shci_notify_asynch_evt(void* pdata)
{
  UNUSED(pdata);
  SCH_SetTask(1U << CFG_TASK_SYSTEM_HCI_ASYNCH_EVT, CFG_SCH_PRIO_0);
  return;
}

void shci_cmd_resp_release(uint32_t flag)
{
  UNUSED(flag);
  SCH_SetEvt(1U << CFG_EVT_SYSTEM_HCI_CMD_EVT_RESP);
  return;
}

void shci_cmd_resp_wait(uint32_t timeout)
{
  UNUSED(timeout);
  SCH_WaitEvt(1U << CFG_EVT_SYSTEM_HCI_CMD_EVT_RESP);
  return;
}

/* Received trace buffer from M0 */
void TL_TRACES_EvtReceived( TL_EvtPacket_t * hcievt )
{
#if(CFG_DEBUG_TRACE != 0)
  /* Call write/print function using DMA from dbg_trace */
  /* - Cast to TL_AsynchEvt_t* to get "real" payload (without Sub Evt code 2bytes),
     - (-2) to size to remove Sub Evt Code */
  DbgTraceWrite(1U, (const unsigned char *) ((TL_AsynchEvt_t *)(hcievt->evtserial.evt.payload))->payload, hcievt->evtserial.evt.plen - 2U);
#endif /* CFG_DEBUG_TRACE */
  /* Release buffer */
  TL_MM_EvtDone( hcievt );
}
/**
  * @brief  Initialisation of the trace mechanism
  * @param  None
  * @retval None
  */
#if(CFG_DEBUG_TRACE != 0)
void DbgOutputInit( void )
{
  HW_UART_Init(DBG_TRACE_UART_CFG);
  return;
}

/**
  * @brief  Management of the traces
  * @param  p_data : data
  * @param  size : size
  * @param  call-back :
  * @retval None
  */
void DbgOutputTraces(  uint8_t *p_data, uint16_t size, void (*cb)(void) )
{
  HW_UART_Transmit_DMA(DBG_TRACE_UART_CFG, p_data, size, cb);

  return;
}
#endif

/* USER CODE BEGIN FD_WRAP_FUNCTIONS */
/**
  * @brief This function manage the Push button action
  * @param  GPIO_Pin : GPIO pin which has been activated
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  APP_DBG("*** HAL_GPIO_EXTI_Callback  GPIO_Pin = %d ****", GPIO_Pin);
  switch(GPIO_Pin)
  {
    case BUTTON_SW1_PIN:
        SCH_SetTask(TASK_COAP_MSG_BUTTON,CFG_SCH_PRIO_1);
        break;

     default:
        break;
  }
}
/* USER CODE END FD_WRAP_FUNCTIONS */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
