#pragma once

#include <spdlog/spdlog.h>

namespace QuickGit
{
	struct Log
	{
		static void Init();
		static spdlog::logger* GetLogger();
	};
}

#define QG_LOG_TRACE(...)		::QuickGit::Log::GetLogger()->trace(__VA_ARGS__)
#define QG_LOG_INFO(...)		::QuickGit::Log::GetLogger()->info(__VA_ARGS__)
#define QG_LOG_DEBUG(...)		::QuickGit::Log::GetLogger()->debug(__VA_ARGS__)
#define QG_LOG_WARN(...)		::QuickGit::Log::GetLogger()->warn(__VA_ARGS__)
#define QG_LOG_ERROR(...)		::QuickGit::Log::GetLogger()->error(__VA_ARGS__)
#define QG_LOG_CRITICAL(...)	::QuickGit::Log::GetLogger()->critical(__VA_ARGS__)
