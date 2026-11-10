// Logging. Messages go to stderr and to optional listeners (console, tests).
#pragma once
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace ose {

enum class LogLevel { Debug, Info, Warning, Error };

void logMessage(LogLevel level, std::string_view msg);
void setLogLevel(LogLevel minimum);
// Listener receives every message at or above the minimum level. Returns an id for removal.
int addLogListener(std::function<void(LogLevel, std::string_view)> fn);
void removeLogListener(int id);

template <class... Args>
void logDebug(std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void logInfo(std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void logWarn(std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Warning, std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
void logError(std::format_string<Args...> fmt, Args&&... args) {
    logMessage(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
}

// Log a warning only the first time a given key is seen (unknown shaders, proxies...).
void warnOnce(const std::string& key, std::string_view msg);

}  // namespace ose
