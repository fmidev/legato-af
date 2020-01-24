#include "fmiSmsCtrl.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <openssl/md5.h>

namespace {
    constexpr uint64_t magic_number = 17854383639879076;
    const char* DELIM = " \t\r\n";
}

Fmi::SmsCtrl::SmsCtrl(const std::string& allowed_phone_list_fn)
    : magic(magic_number)
{
    read_phone_list(allowed_phone_list_fn);
    create_pin("/etc/fmi/smsRebootSalt");
    setup_sms_handler();
}

Fmi::SmsCtrl::~SmsCtrl()
{
}

bool Fmi::SmsCtrl::is_phone_allowed(const std::string& phone_num) const
{
    return allowed_phones.empty() or allowed_phones.count(phone_num);
}

void Fmi::SmsCtrl::read_phone_list(const std::string& allowed_phone_list_fn)
{
    std::string line;
    std::ifstream input(allowed_phone_list_fn.c_str());
    if (not input) {
        LE_WARN("Cannot open %s: %s", allowed_phone_list_fn.c_str(), strerror(errno));
    } else {
        while (std::getline(input, line)) {
            std::string src = line;
            std::size_t pos = src.find_first_not_of(DELIM);
            if (pos != std::string::npos) {
                src = src.substr(pos);
            }
            pos = src.find_first_of(DELIM);
            if (pos != std::string::npos) {
                src = src.substr(0, pos);
            }
            if (src.length() > 0) {
                if (*src.begin() == '#') {
                    LE_INFO("COMMENT: %s", line.c_str());
                } else {
                    // FIXME: should we check whether begins with '+'?
                    allowed_phones.insert(src);
                    LE_INFO("Allowed phone: '%s'", src.c_str());
                }
            }
        }
    }

    if (allowed_phones.empty()) {
        LE_WARN("No allowed phone list provided (%s) or it os empty", allowed_phone_list_fn.c_str());
        LE_INFO("Accepting SMS from any phone");
    }
}

void Fmi::SmsCtrl::create_pin(const std::string& salt_fn)
{
    std::string tmp;
    std::ifstream input(salt_fn.c_str());
    if (not input) {
        LE_WARN("Cannot open %s: %s", salt_fn.c_str(), strerror(errno));
    } else {
        std::copy(
            std::istreambuf_iterator<char>(input.rdbuf()),
            std::istreambuf_iterator<char>(),
            std::back_inserter(tmp));
    }

    if (tmp.length() == 0) {
        return;
    }

    char serial_number[LE_INFO_MAX_PSN_BYTES] = {0};
    le_info_GetPlatformSerialNumber(serial_number, sizeof(serial_number));
    LE_INFO("Serial number: %s", serial_number);

    MD5_CTX ctx;
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5_Init(&ctx);
    MD5_Update(&ctx, (unsigned char*)tmp.c_str(), tmp.length());
    MD5_Update(&ctx, serial_number, strlen(serial_number));
    MD5_Final(md, &ctx);

    std::ostringstream hmd;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        char buf[3];
        snprintf(buf, 3, "%02X", md[i]);
        hmd << buf;
    }
    LE_INFO("Generated hash: %s", hmd.str().c_str());
    this->hmd = hmd.str();
}

void Fmi::SmsCtrl::setup_sms_handler()
{
    le_sms_AddRxMessageHandler(&Fmi::SmsCtrl::on_rx_message, this);
    le_sms_AddFullStorageEventHandler(&Fmi::SmsCtrl::on_storage_message, this);
}

void Fmi::SmsCtrl::handle_sms_message(const std::string& phone, const std::string& text)
{
    if (not is_phone_allowed(phone)) {
        LE_INFO("SMS from %s not accepted (phone not in the list)", phone.c_str()); 
    }

    if (strncasecmp(text.c_str(), "reboot", 6) == 0) {
        const char* w1 = text.c_str() + 6;
        while (*w1 and std::isspace(*w1)) { w1++; }
        if (not *w1) {
            LE_INFO("No pin found in SMS text: '%s'", text.c_str());
            return;
        }
        const char* w2 = w1;
        while (*w2 and not std::isspace(*w2)) { w2++; }
        const std::string pin(w1, w2);
        LE_INFO("Found PIN '%s'", pin.c_str());
        if (pin.length() >= 4 and pin.length() <= hmd.length()) {
            if (strncasecmp(pin.c_str(), hmd.c_str(), pin.length()) == 0) {
                le_framework_NotifyExpectedReboot();
                LE_INFO("Rebooting...");
                kill(getpid(), SIGINT);
            } else {
                LE_INFO("Got pin '%s' (expected at least 4 first symbols of '%s')", pin.c_str(), hmd.c_str());
            }
        }
    }
}

void Fmi::SmsCtrl::on_rx_message(le_sms_MsgRef_t msgRef, void* contextPtr)
{
    LE_INFO("on_rx_message entered");

    assert(contextPtr);
    Fmi::SmsCtrl* ctrl = reinterpret_cast<Fmi::SmsCtrl*>(contextPtr);
    assert(ctrl->magic == magic_number);

    std::string phone;
    std::string text;

    try {
        char buffer[256];
        le_result_t result;

        result = le_sms_GetSenderTel(msgRef, buffer, sizeof(buffer));
        if (result == LE_OK) {
            phone = buffer;
        } else {
            throw std::runtime_error("le_sms_GetSenderTel: error code " + std::to_string(int(result)));
        }

        LE_INFO("Got SMS from %s\n", phone.c_str());

        if (le_sms_GetFormat(msgRef) != LE_SMS_FORMAT_TEXT) {
            throw std::runtime_error("only ASCII messages accepted");
        }

        result = le_sms_GetText(msgRef, buffer, sizeof(buffer));
        if (result == LE_OK) {
            text = buffer;
        } else {
            throw std::runtime_error("le_sms_GetText: error code " + std::to_string(int(result)));
        }

        le_sms_Delete(msgRef);

        ctrl->handle_sms_message(phone, text);
    } catch (const std::exception& e) {
        LE_ERROR("Fmi::SmsCtrl::on_rx_message: %s", e.what());
    }
}

void Fmi::SmsCtrl::on_storage_message(le_sms_Storage_t storage, void* contextPtr)
{
  LE_INFO("A Full storage SMS message is received. Type of full storage %d", storage);
}
