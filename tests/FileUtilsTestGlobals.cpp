#include <string>

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
typedef void (*LogCallback)(const std::string &msg, LogLevel level);

int g_currentLogLevel = LOG_INFO;
LogCallback g_logCallback = nullptr;
