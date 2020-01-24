#include "fmiSmsCtrl.hpp"
#include <memory>

std::unique_ptr<Fmi::SmsCtrl> smsCtrl;

COMPONENT_INIT
{
    const char* PHONE_LIST_FN = "/etc/fmi/fx30s_sms.list";
    LE_INFO("fmiSmsCtrl started");
    smsCtrl.reset(new Fmi::SmsCtrl(PHONE_LIST_FN));
}
