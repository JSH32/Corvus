#include "corvus/log.hpp"
#include "raylib.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace Corvus {
void spdCustomLog(int msgType, const char* text, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);

    int size = std::vsnprintf(nullptr, 0, text, args_copy);
    va_end(args_copy);

    if (size <= 0) {
        return;
    }

    std::vector<char> buf(size + 1);
    std::vsnprintf(buf.data(), buf.size(), text, args);

    std::string output(buf.data());

    switch (msgType) {
        case LOG_INFO:
            CORVUS_CORE_INFO(output);
            break;
        case LOG_ERROR:
            CORVUS_CORE_ERROR(output);
            break;
        case LOG_WARNING:
            CORVUS_CORE_WARN(output);
            break;
        case LOG_DEBUG:
            CORVUS_CORE_TRACE(output);
            break;
        default:
            break;
    }
}
std::shared_ptr<spdlog::logger> Log::coreLogger;
std::shared_ptr<spdlog::logger> Log::clientLogger;

void Log::init() {
    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("corvus.log", true));

    logSinks[0]->set_pattern("%^[%T] %n: %v%$");
    logSinks[1]->set_pattern("[%T] [%l] %n: %v");

    coreLogger = std::make_shared<spdlog::logger>("LINP", begin(logSinks), end(logSinks));
    spdlog::register_logger(coreLogger);
    coreLogger->set_level(spdlog::level::trace);
    coreLogger->flush_on(spdlog::level::trace);

    clientLogger = std::make_shared<spdlog::logger>("APP", begin(logSinks), end(logSinks));
    spdlog::register_logger(clientLogger);
    clientLogger->set_level(spdlog::level::trace);
    clientLogger->flush_on(spdlog::level::trace);

    SetTraceLogCallback(spdCustomLog);
}
}
