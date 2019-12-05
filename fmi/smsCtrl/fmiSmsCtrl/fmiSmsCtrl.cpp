#include "legato.h"
#include "interfaces.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define MAX_ALLOWED_PHONES 100

size_t num_phones = 0;
char *allowed_phones[MAX_ALLOWED_PHONES];

void read_phone_list()
{
  char *buffer = NULL;
  size_t buffer_size = 0;
  size_t len, i;
  FILE* input = fopen("/etc/fmi/fx30s_sms.list", "r");
  if (input) {
    while ((len = getline(&buffer, &buffer_size, input)) != (size_t)-1) {
      LE_INFO("LEN: %d\n", (int)len);
      char *a = NULL, *b = NULL;
      for (i = 0; i < len && isspace(buffer[i]); i++) {}
      a = buffer + i;
      if (i < len && *a == '#') {
        LE_INFO("COMMENT: %s\n", a);
      } else {
        for (i = 0; i < len && !isspace(buffer[i]); i++) {}
        b = buffer + i;
        if (a != b) {
            char *item = (char*)malloc(b - a + 1);
          memcpy(item, a, b - a);
          item[b - a] = 0;
        if (num_phones >= MAX_ALLOWED_PHONES) {
          assert(0);
        } else {
          LE_INFO("Allowed phone: '%s'", item);
          allowed_phones[num_phones++] = item;
        }
        }
      }
    }
  } else {
    LE_INFO("Cannot open /etc/fmi/fx30s_sms.list: %s", strerror(errno));
  }
}

size_t lookup_phone(const char* text)
{
  if (num_phones) {
    size_t i;
    for (i = 0; i < num_phones; i++) {
      if (strcmp(text, allowed_phones[i]) == 0) {
        return 1;
      }
    }
    return 0;
  } else {
    return 1;
  }
}

void on_rx_message(le_sms_MsgRef_t msgRef, void* contextPtr)
{
  char buffer[256];
  le_result_t result;
  LE_INFO("on_rx_message entered");
  result = le_sms_GetSenderTel(msgRef, buffer, sizeof(buffer));
  if (result == LE_OK) {
    if (lookup_phone(buffer)) {
      LE_INFO("Got SMS from %s\n", buffer);
      if (le_sms_GetFormat(msgRef) == LE_SMS_FORMAT_TEXT) {
        le_sms_GetText(msgRef, buffer, sizeof(buffer));
        if (result == LE_OK) {
          LE_INFO("Received text is '%s'\n", buffer);
          if (strncasecmp(buffer, "reboot", 6) == 0) {
            LE_INFO("Rebooting...");
            kill(getpid(), SIGINT);
          }
        } else {
          LE_ERROR("Only ASCII messages accepted");
        }
      }
    }
    le_sms_Delete(msgRef);
  } else {
    LE_ERROR("Return code: %d\n", (int)result);
  }
}

void on_storage_message(le_sms_Storage_t storage, void* contextPtr)
{
  LE_INFO("A Full storage SMS message is received. Type of full storage %d", storage);
}

COMPONENT_INIT
{
  LE_INFO("fmiSmsCtrl started");
  read_phone_list();
  le_sms_ConnectService();
  //LE_INFO("le_smsInbox_ConnectService done");
  le_sms_AddRxMessageHandler(&on_rx_message, 0);
  LE_INFO("le_smsInbox handler installed");
  le_sms_AddFullStorageEventHandler(on_storage_message, 0);
}
