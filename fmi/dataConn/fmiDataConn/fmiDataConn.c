#include "legato.h"
#include "interfaces.h"
#include <stdio.h>

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
}
