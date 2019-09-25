#include "legato.h"
#include "interfaces.h"
#include <stdio.h>
#include "watchdogChain.h"

#define MS_WDOG_INTERVAL 8

le_data_RequestObjRef_t req;

void log_connection_state(const char* interfaceName, bool isConnected, void* contextPtr)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "%s is now %s connected",
           interfaceName,
           (isConnected ? "" : "not"));
  LE_INFO(buffer);
}

COMPONENT_INIT
{
  le_data_AddConnectionStateHandler(&log_connection_state, NULL);
  le_data_ConnectService();
  req = le_data_Request();

  // Try to kick a couple of times before each timeout.
  // (borrowed from $LEGATO_ROOT/components/audio/le_audio.c)

  le_clk_Time_t watchdogInterval = { .sec = MS_WDOG_INTERVAL };
  le_wdogChain_Init(1);
  le_wdogChain_MonitorEventLoop(0, watchdogInterval);

}
