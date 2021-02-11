#include "DataConnMonitor.hpp"
#include <cstdio>
#include <iostream>
#include <iterator>
#include <fstream>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include "watchdogChain.h"
}

#define MS_WDOG_INTERVAL 8

std::string required_apn = "";

std::string read_apn()
{
    try {
        std::ifstream input;
        input.exceptions(std::ios::failbit|std::ios::badbit);
        input.open("/etc/fmi/apn.dat");
        input.exceptions(std::ios::goodbit);
        std::string tmp;
        std::copy(
            std::istreambuf_iterator<char>(input.rdbuf()),
            std::istreambuf_iterator<char>(),
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



std::unique_ptr<DataConnectionMonitor> monitor;

COMPONENT_INIT
{
    required_apn = read_apn();
    monitor.reset(new DataConnectionMonitor(required_apn.c_str()));

  // Try to kick a couple of times before each timeout.
  // (borrowed from $LEGATO_ROOT/components/audio/le_audio.c)

  le_clk_Time_t watchdogInterval = { .sec = MS_WDOG_INTERVAL };
  le_wdogChain_Init(1);
  le_wdogChain_MonitorEventLoop(0, watchdogInterval);
}
