#include <windows.h>
#include "../src/Globals.inl"
#include <iostream>

int g_currentLogLevel = LOG_INFO;

void DebugLog(const std::string &msg, LogLevel level) {
    if (level >= g_currentLogLevel) {
        std::cout << "[LOG] " << msg << std::endl;
    }
}
