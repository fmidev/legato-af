#ifndef DATA_CONNECTION_MONITOR_HPP_
#define DATA_CONNECTION_MONITOR_HPP_

#include "legato.h"
#include "interfaces.h"
#include <atomic>
#include <string>
#include "le_timer.h"
#include "le_ulpm_interface.h"

class DataConnectionMonitor final
{
public:
    DataConnectionMonitor(const std::string& apn);
    virtual ~DataConnectionMonitor();

private:
    void on_notification_timer();

    void on_network_registration_monitor_timer();

    void on_net_reg_state(le_mrc_NetRegState_t state);

    void on_connection_state_change(const char* interfaceName, bool isConnected);

    void on_signal_strength_change(int32_t ss);

    uint32_t get_profile_in_use();

    /**
     *   @brief checks actual APN (returns OK also if failed to get APN)
     */
    bool check_apn();

    void reboot();

private:
    bool done;
    le_mrc_NetRegState_t net_reg_state;
    bool data_connection_ok;
    double net_reg_state_changed_time;
    double net_reg_home_last_time;
    double data_connection_ok_time;
    uint32_t timer_count;

    /**
     *   Required APN value
     */
    const std::string apn;

    /**
     *   Monitoring thread
     */
    le_thread_Ref_t radio_monitor_thread;

    le_thread_Ref_t state_notification_thread;

    static void* radio_monitor_thread_wrapper(void* context);
    static void on_net_reg_state_wrapper(le_mrc_NetRegState_t state, void* context);
    static void on_connection_state_change_wrapper(const char* interfaceName, bool isConnected, void* context);
    static void on_notification_timer_wrapper(le_timer_Ref_t timerRef);
    static void on_network_registration_monitor_timer_wrapper(le_timer_Ref_t timer_ref);

    le_data_ConnectionStateHandlerRef_t data_connection_state_handler_ref;
    le_mrc_NetRegStateEventHandlerRef_t net_reg_state_event_handler_ref;
    le_data_RequestObjRef_t data_req;
    le_timer_Ref_t notification_timer;
    le_timer_Ref_t net_reg_monitor_timer;
    le_pm_WakeupSourceRef_t wakeup_source;
    bool tried_radio_off_on;
};

#endif // DATA_CONNECTION_MONITOR_HPP_
