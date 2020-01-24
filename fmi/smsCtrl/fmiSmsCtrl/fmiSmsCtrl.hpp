#pragma once

#include "legato.h"
#include "interfaces.h"
#include <set>
#include <string>

namespace Fmi {

    class SmsCtrl
    {
    public:
        SmsCtrl(const std::string& allowed_phone_list_fn);
        virtual ~SmsCtrl();

    private:
        bool is_phone_allowed(const std::string& phone_num) const;

        void read_phone_list(const std::string& allowed_phone_list_fn);

        void create_pin(const std::string& salt_fn);

        void setup_sms_handler();

        void handle_sms_message(const std::string& phone, const std::string& text);

        static void on_rx_message(le_sms_MsgRef_t msgRef, void* contextPtr);

        static void on_storage_message(le_sms_Storage_t storage, void* contextPtr);
    private:
        const uint64_t magic;
        std::set<std::string> allowed_phones;
        std::string hmd;
    };

} // namespace Fmi

