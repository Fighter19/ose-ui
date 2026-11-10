#include "core/log.h"

#include <cstdio>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace ose {
namespace {

struct LogState {
    std::mutex mutex;
    LogLevel minimum = LogLevel::Info;
    int nextId = 1;
    std::vector<std::pair<int, std::function<void(LogLevel, std::string_view)>>> listeners;
    std::unordered_set<std::string> onceKeys;
};

LogState& state() {
    static LogState s;
    return s;
}

const char* prefix(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "[debug] ";
        case LogLevel::Info: return "";
        case LogLevel::Warning: return "[warn] ";
        case LogLevel::Error: return "[error] ";
    }
    return "";
}

}  // namespace

void logMessage(LogLevel level, std::string_view msg) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    if (level < s.minimum) return;
    std::fprintf(stderr, "%s%.*s\n", prefix(level), int(msg.size()), msg.data());
    for (auto& [id, fn] : s.listeners) fn(level, msg);
}

void setLogLevel(LogLevel minimum) {
    std::lock_guard lock(state().mutex);
    state().minimum = minimum;
}

int addLogListener(std::function<void(LogLevel, std::string_view)> fn) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    int id = s.nextId++;
    s.listeners.emplace_back(id, std::move(fn));
    return id;
}

void removeLogListener(int id) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    std::erase_if(s.listeners, [id](auto& p) { return p.first == id; });
}

void warnOnce(const std::string& key, std::string_view msg) {
    {
        auto& s = state();
        std::lock_guard lock(s.mutex);
        if (!s.onceKeys.insert(key).second) return;
    }
    logMessage(LogLevel::Warning, msg);
}

}  // namespace ose
