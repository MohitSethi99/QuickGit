#include "pch.h"
#include "Log.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

namespace QuickGit
{
	static std::shared_ptr<spdlog::logger> s_Logger;

	void Log::Init()
	{
		eastl::vector<spdlog::sink_ptr> logSinks;
		logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("QuickGit.log", true));
		logSinks.emplace_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());

		logSinks[0]->set_pattern("%^[%T] %n: %v%$");
		logSinks[1]->set_pattern("[%T] [%l] %n: %v");
		logSinks[2]->set_pattern("%^[%T] [%l] %n: %v%$");

		s_Logger = std::make_shared<spdlog::logger>("QUICKGIT", logSinks.begin(), logSinks.end());
		spdlog::register_logger(s_Logger);
		s_Logger->set_level(spdlog::level::trace);
	}

	spdlog::logger* Log::GetLogger()
	{
		return s_Logger.get();
	}
}
