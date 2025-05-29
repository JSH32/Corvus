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

namespace Linp {
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

#define LINP_CORE_TRACE(...)    ::Linp::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LINP_CORE_INFO(...)     ::Linp::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LINP_CORE_WARN(...)     ::Linp::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LINP_CORE_ERROR(...)    ::Linp::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LINP_CORE_CRITICAL(...) ::Linp::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define LINP_TRACE(...)    ::Linp::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LINP_INFO(...)     ::Linp::Log::GetClientLogger()->info(__VA_ARGS__)
#define LINP_WARN(...)     ::Linp::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LINP_ERROR(...)    ::Linp::Log::GetClientLogger()->error(__VA_ARGS__)
#define LINP_CRITICAL(...) ::Linp::Log::GetClientLogger()->critical(__VA_ARGS__)
