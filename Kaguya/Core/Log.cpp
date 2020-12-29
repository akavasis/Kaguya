#include "pch.h"
#include "Log.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void Log::Create()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");
	s_Logger = spdlog::stdout_color_mt("ENGINE");
	s_Logger->set_level(spdlog::level::trace);
}