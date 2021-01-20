#include "legato.h"
#include "interfaces.h"
#include <cstdio>
#include <iostream>
#include <iterator>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <le_thread.h>

extern "C" {
#include "watchdogChain.h"
}

#define MS_WDOG_INTERVAL 8

le_data_RequestObjRef_t req;
uint32_t current_profile;
std::string required_apn = "";

std::string read_apn()
{
    try {
        std::ifstream input;
        input.exceptions(std::ios::failbit|std::ios::badbit);
        input.open("/etc/fmi/apn.dat");
        input.exceptions(std::ios::goodbit);
        std::string tmp;
        std::copy(std::istreambuf_iterator<char>(input.rdbuf()), std::istreambuf_iterator<char>(),
            std::back_inserter(tmp));
        std::size_t n = tmp.find_first_not_of(" \t\r\n");
        if (n != std::string::npos and n > 0) {
            tmp = tmp.substr(n);
        }
        n = tmp.find_first_of(" \t\r\n");
        if (n != std::string::npos) {
            tmp = tmp.substr(0, n);
        }
        return tmp;
    } catch (const std::exception&e ) {
        return "";
    }
}

uint32_t getProfileInUse()
{
    const char* PROFILE_IN_USE = "tools/cmodem/profileInUse";
    uint32_t profileIndex;
    le_cfg_IteratorRef_t iteratorRef = le_cfg_CreateReadTxn(PROFILE_IN_USE);

    // if node does not exist, use the default profile
    if (!le_cfg_NodeExists(iteratorRef, ""))
    {
        profileIndex = LE_MDC_DEFAULT_PROFILE;
    }
    else
    {
        profileIndex = le_cfg_GetInt(iteratorRef, "", LE_MDC_DEFAULT_PROFILE);
    }

    le_cfg_CancelTxn(iteratorRef);

    return profileIndex;
}

void verify_apn()
{
    le_mdc_ProfileRef_t profile = le_mdc_GetProfile(current_profile);
    if (profile) {
        char buffer[256];
        le_result_t res = le_mdc_GetAPN(profile, buffer, sizeof(buffer));
        if (res == LE_OK) {
            const std::string apn = buffer;
            if (apn != required_apn) {
                LE_INFO("Found APN '%s', expected '%s'", apn.c_str(), required_apn.c_str());
                res = le_mdc_SetAPN(profile, required_apn.c_str());
                if (res == LE_OK) {
                    LE_INFO("Changed APN to '%s'", required_apn.c_str());
                } else {
                    LE_ERROR("Failed to set APN - error %d", res);
                }
            }
        } else {
            LE_ERROR("Failed to query APN - error code %d", res);
        }
    }
}

void set_led(bool on)
{
  if (on) {
    le_gpioPin48_Deactivate();
    le_gpioPin47_Activate();
  } else {
    le_gpioPin48_Activate();
    le_gpioPin47_Deactivate();
  }
}

bool connected = false;

void log_connection_state(const char* interfaceName, bool isConnected, void* contextPtr)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "%s is now %s connected",
           interfaceName,
           (isConnected ? "" : "not"));
  LE_INFO(buffer);
  set_led(isConnected);
  connected = isConnected;
}

void* radio_monitor_thread_proc(void* context)
{
    const int period = 10;
    const int max_offline_time = 3600;
    int offline_time = 0;

    le_mrc_ConnectService();

    while (true) {
        le_result_t res;
        le_onoff_t onoff;

        usleep(1000000*period);

        res = le_mrc_GetRadioPower(&onoff);
        if (res != LE_OK){
            continue;
        } else if (res == LE_OK) {
            if (onoff == LE_OFF) {
                le_mrc_SetRadioPower(LE_ON);
                offline_time = 0;
            } else {
                if (connected) {
                    offline_time = 0;
                } else {
                    offline_time += period;
                }

                if (offline_time > max_offline_time) {
                    le_mrc_SetRadioPower(LE_OFF);
                }
            }
        }
    }
}

COMPONENT_INIT
{
  le_thread_Ref_t radio_monitor_thread;

  set_led(false);

  radio_monitor_thread = le_thread_Create("RadioMonitorThread", &radio_monitor_thread_proc, nullptr);
  le_thread_Start(radio_monitor_thread);

  current_profile = getProfileInUse();
  LE_INFO("Current prifile: %d", (int)current_profile);
  required_apn = read_apn();
  if (required_apn != "") {
      verify_apn();
  }

  le_data_AddConnectionStateHandler(&log_connection_state, NULL);
  le_data_ConnectService();
  req = le_data_Request();

  // Try to kick a couple of times before each timeout.
  // (borrowed from $LEGATO_ROOT/components/audio/le_audio.c)

  le_clk_Time_t watchdogInterval = { .sec = MS_WDOG_INTERVAL };
  le_wdogChain_Init(1);
  le_wdogChain_MonitorEventLoop(0, watchdogInterval);

}
