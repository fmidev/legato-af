#include "legato.h"
#include "interfaces.h"
#include <stdio.h>

le_pm_WakeupSourceRef_t ws;

COMPONENT_INIT
{
  le_pm_ConnectService();
  ws = le_pm_NewWakeupSource(0, "FMI_KeepAwake");
  switch (le_pm_StayAwake(ws)) {
  case LE_OK:
    LE_INFO("Staying awake");
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
