#pragma once

#if defined(_WIN32)
#define NOGDI  // All GDI defines and routines
#define NOUSER // All USER defines and routines
#endif

#include "spdlog/logger.h"

#if defined(_WIN32) // raylib uses these names as function parameters
#undef near
#undef far
#endif

namespace Corvus {
/**
 * @brief Custom adapter for raylib to our logging system.
 */
void spdCustomLog(int msgType, const char* text, va_list args);

class Log {
public:
    static void init();

    static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return coreLogger; }
    static std::shared_ptr<spdlog::logger>& GetClientLogger() { return clientLogger; }

private:
    static std::shared_ptr<spdlog::logger> coreLogger;
    static std::shared_ptr<spdlog::logger> clientLogger;
};
}

#define CORVUS_CORE_TRACE(...)    ::Corvus::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define CORVUS_CORE_INFO(...)     ::Corvus::Log::GetCoreLogger()->info(__VA_ARGS__)
#define CORVUS_CORE_WARN(...)     ::Corvus::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define CORVUS_CORE_ERROR(...)    ::Corvus::Log::GetCoreLogger()->error(__VA_ARGS__)
#define CORVUS_CORE_CRITICAL(...) ::Corvus::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define CORVUS_TRACE(...)    ::Corvus::Log::GetClientLogger()->trace(__VA_ARGS__)
#define CORVUS_INFO(...)     ::Corvus::Log::GetClientLogger()->info(__VA_ARGS__)
#define CORVUS_WARN(...)     ::Corvus::Log::GetClientLogger()->warn(__VA_ARGS__)
#define CORVUS_ERROR(...)    ::Corvus::Log::GetClientLogger()->error(__VA_ARGS__)
#define CORVUS_CRITICAL(...) ::Corvus::Log::GetClientLogger()->critical(__VA_ARGS__)
