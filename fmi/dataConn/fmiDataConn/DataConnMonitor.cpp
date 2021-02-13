#include "DataConnMonitor.hpp"
#include <cassert>
#include <ctime>
#include <iostream>

#include <cerrno>

namespace {

    double get_clock()
    const double RADIO_OFF_ON_TIME = 3600.0;
    const double REBOOT_TIME = 7200.0;
#endif
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + 1.0e-09*ts.tv_nsec;
    }

    template <typename DataType>
    void update(DataType* data, const DataType newValue, double* clock)
    {
        if (*data != newValue) {
            *data = newValue;
            *clock = get_clock();
        }
    }

    le_clk_Time_t mk_time(unsigned millisec)
    {
        le_clk_Time_t interval;
        interval.sec = millisec / 1000;
        interval.usec = 1000 * (millisec % 1000);
        return interval;
    }
}

DataConnectionMonitor::DataConnectionMonitor(const std::string& apn)
    : done(false)
    , net_reg_state(LE_MRC_REG_NONE)
    , data_connection_ok(false)
    , apn(apn)
    , tried_radio_off_on(false)
{
    double now = get_clock();
    net_reg_state_changed_time = now;
    net_reg_home_last_time = now;
    data_connection_ok_time = now;

    le_appCtrl_ConnectService();
    le_appInfo_ConnectService();
    le_data_ConnectService();
    le_mrc_ConnectService();
    le_pm_ConnectService();
    le_ulpm_ConnectService();

    wakeup_source = le_pm_NewWakeupSource(0, "FMI_KeepAwake");

    net_reg_state_event_handler_ref =
        le_mrc_AddNetRegStateEventHandler(&DataConnectionMonitor::on_net_reg_state_wrapper, this);

    data_connection_state_handler_ref =
        le_data_AddConnectionStateHandler(&DataConnectionMonitor::on_connection_state_change_wrapper, this);
    data_req = le_data_Request();

    notification_timer = le_timer_Create("StateNotificationTimer");
    le_timer_SetContextPtr(notification_timer, this);
    le_timer_SetInterval(notification_timer, mk_time(500));
    le_timer_SetHandler(notification_timer, &DataConnectionMonitor::on_notification_timer_wrapper);
    le_timer_SetRepeat(notification_timer, 0);
    le_timer_Start(notification_timer);

    net_reg_monitor_timer = le_timer_Create("NetworkRegistrationMonitorTimer");
    le_timer_SetContextPtr(net_reg_monitor_timer, this);
    le_timer_SetInterval(net_reg_monitor_timer, mk_time(5000));
    le_timer_SetHandler(net_reg_monitor_timer, &DataConnectionMonitor::on_network_registration_monitor_timer_wrapper);
    le_timer_SetRepeat(net_reg_monitor_timer, 0);
    le_timer_Start(net_reg_monitor_timer);

    le_mrc_GetNetRegState(&net_reg_state);
}

DataConnectionMonitor::~DataConnectionMonitor()
{
    if (data_connection_state_handler_ref) {
        le_data_RemoveConnectionStateHandler(data_connection_state_handler_ref);
    }
}

void DataConnectionMonitor::on_network_registration_monitor_timer()
{
    le_onoff_t onoff;
    double now = get_clock();

    LE_ERROR_IF(le_mrc_GetRadioPower(&onoff) != LE_OK, "Failed to query radio state");
    //LE_INFO("radio=%d", (int)onoff);
    if (onoff == LE_OFF) {
        // Radio turned of -> turn it on
        LE_INFO("Radio was off: turning it ON");
        LE_ERROR_IF(le_mrc_SetRadioPower(LE_ON) != LE_OK, "Failed to turn radio on");
    } else {
        // Radio is on
        if (net_reg_state == LE_MRC_REG_HOME) {
            // home network
            tried_radio_off_on = false;

            // Check APN if desired value provided and update if required
            if (apn != "") {
                uint32_t current_profile = get_profile_in_use();
                le_mdc_ProfileRef_t profile = le_mdc_GetProfile(current_profile);
                if (profile) {
                    char buffer[256];
                    le_result_t res = le_mdc_GetAPN(profile, buffer, sizeof(buffer));
                    if (res == LE_OK) {
                        if (std::string(buffer) != apn) {
                            LE_INFO("APN %s wanted and %s found: trying to change",
                                apn.c_str(), buffer);
                            LE_ERROR_IF(le_mrc_SetRadioPower(LE_OFF) != LE_OK,
                                "Failed to turn radio off");
                            LE_ERROR_IF(le_mdc_SetAPN(profile, apn.c_str()) != LE_OK,
                                "Failed to set APN");
                        }
                    } else {
                        LE_ERROR("Failed to get APN");
                    }
                }
            }

        } else if (not tried_radio_off_on and (now - net_reg_home_last_time > RADIO_OFF_ON_TIME)) {
            // Not get home network for an hour
            tried_radio_off_on = true;
            LE_INFO("Not registered to network for 1 hour -> turning radio OFF");
            LE_ERROR_IF(le_mrc_SetRadioPower(LE_OFF) != LE_OK, "Failed to turn radio off");
        } else if (now - net_reg_home_last_time > REBOOT_TIME) {
            LE_INFO("Not registered to network for 2 hour -> trying to reboot");
            reboot();
        }
    }
}

void DataConnectionMonitor::on_notification_timer()
{
    if (data_connection_ok) {
        le_gpioPin48_Deactivate();
        le_gpioPin47_Activate();
        data_connection_ok_time = get_clock();
    } else if (net_reg_state == LE_MRC_REG_HOME) {
        le_gpioPin48_Activate();
        le_gpioPin47_Deactivate();
    } else {
        le_gpioPin47_Deactivate();
        if (timer_count % 2) {
            le_gpioPin48_Activate();
        } else {
            le_gpioPin48_Deactivate();
        }
        timer_count++;
    }
}

void DataConnectionMonitor::on_net_reg_state(le_mrc_NetRegState_t state)
{
    const char* desc = "";
    switch (state) {
    case LE_MRC_REG_NONE:
        desc = "Not registered and not currently searching for new operator";
        break;
    case LE_MRC_REG_HOME:
        desc = "Registered, home network";
        update(&net_reg_state, state, &net_reg_home_last_time);
        break;
    case LE_MRC_REG_SEARCHING:
        desc = "Not registered but currently searching for a new operator";
        break;
    case LE_MRC_REG_DENIED:
        desc = "Registration was denied, usually because of invalid access credentials";
        break;
    case LE_MRC_REG_ROAMING:
        desc = "Registered to a roaming network";
        break;
    case LE_MRC_REG_UNKNOWN:
        desc = "Unknown state";
        break;
    }
    LE_INFO("Network registration state: %s", desc);
    update(&net_reg_state, state, &net_reg_state_changed_time);
}

void DataConnectionMonitor::on_connection_state_change(const char* interfaceName, bool isConnected)
{
    char buffer[256];
    data_connection_ok = isConnected;
    snprintf(buffer, sizeof(buffer),
        "%s is now %s connected",
        interfaceName,
        (isConnected ? "" : "not"));
    LE_INFO(buffer);
}

void DataConnectionMonitor::on_signal_strength_change(int32_t ss)
{
    LE_INFO("Signal strength: %d", int(ss));
}

uint32_t DataConnectionMonitor::get_profile_in_use()
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

bool DataConnectionMonitor::check_apn()
{
    if (apn == "") {
        return true;
    } else {
        uint32_t current_profile = get_profile_in_use();
        le_mdc_ProfileRef_t profile = le_mdc_GetProfile(current_profile);
        if (profile) {
            char buffer[256];
            le_result_t res = le_mdc_GetAPN(profile, buffer, sizeof(buffer));
            if (res == LE_OK) {
                const std::string curr_apn = buffer;
                return curr_apn == apn;
            } else {
                LE_ERROR("Failed to get APN - error %d", res);
                // FIXME: should we throw in this case
                return true;
            }
        } else {
            return true;
        }
    }
}

void DataConnectionMonitor::reboot()
{
    const char* appName = "fmiPower";
    if (le_appInfo_GetState(appName) == LE_APPINFO_RUNNING) {
      le_appCtrl_Stop(appName);
    }
    le_pm_Relax(wakeup_source);
    LE_ERROR_IF(le_ulpm_BootOnTimer(10) != LE_OK, "Failed to set boot timer for ULPM");
    LE_ERROR_IF(le_ulpm_ShutDown() != LE_OK, "Failed to enter ULPM");

    // Should not be here: abort to trigger watchdog if still reached
    abort();
}

void DataConnectionMonitor::on_notification_timer_wrapper(le_timer_Ref_t timer_ref)
{
    void* context = le_timer_GetContextPtr(timer_ref);
    DataConnectionMonitor* monitor = reinterpret_cast<DataConnectionMonitor*>(context);
    monitor->on_notification_timer();
}

void DataConnectionMonitor::on_network_registration_monitor_timer_wrapper(le_timer_Ref_t timer_ref)
{
    void* context = le_timer_GetContextPtr(timer_ref);
    DataConnectionMonitor* monitor = reinterpret_cast<DataConnectionMonitor*>(context);
    monitor->on_network_registration_monitor_timer();
}

void DataConnectionMonitor::on_net_reg_state_wrapper(le_mrc_NetRegState_t state, void* context)
{
    DataConnectionMonitor* monitor = reinterpret_cast<DataConnectionMonitor*>(context);
    assert(monitor);
    monitor->on_net_reg_state(state);
}

void DataConnectionMonitor::on_connection_state_change_wrapper(const char* interfaceName, bool isConnected, void* context)
{
    DataConnectionMonitor* monitor = reinterpret_cast<DataConnectionMonitor*>(context);
    assert(monitor);
    monitor->on_connection_state_change(interfaceName, isConnected);
}
