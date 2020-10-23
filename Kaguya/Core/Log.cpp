#include "pch.h"
#include "Log.h"

void Log::Create()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	s_CoreLogger = spdlog::stdout_color_mt("ENGINE");
	s_CoreLogger->set_level(spdlog::level::trace);
}