#include "legato.h"
#include "interfaces.h"
#include "watchdogChain.h"

#define MS_WDOG_INTERVAL 8

le_pm_WakeupSourceRef_t ws;

COMPONENT_INIT
{
  le_pm_ConnectService();
  ws = le_pm_NewWakeupSource(0, "FMI_KeepAwake");
  switch (le_pm_StayAwake(ws)) {
  case LE_OK:
    LE_INFO("Staying awake");

    le_clk_Time_t watchdogInterval = { .sec = MS_WDOG_INTERVAL };
    le_wdogChain_Init(1);
    le_wdogChain_MonitorEventLoop(0, watchdogInterval);

    break;
  case LE_NO_MEMORY:
    LE_ERROR("Not enough memory for acquiring wakeup source");
    break;
  default:
  case LE_FAULT:
    LE_ERROR("Failed to acquire wakeup source");
    break;
  }
}
