#pragma once
#include <spdlog/spdlog.h>

class Log
{
public:
	static void Create();

	inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }
private:
	inline static std::shared_ptr<spdlog::logger> s_Logger;
};

// Core Logging Macros
#define LOG_TRACE(...)		Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)		Log::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)		Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)		Log::GetLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...)	Log::GetLogger()->critical(__VA_ARGS__)