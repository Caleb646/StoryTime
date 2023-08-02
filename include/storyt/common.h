

#if STORYT_BUILD_SPDLOG_ == true
	#include <spdlog/spdlog.h>
	#define STORYT_TRACE(...) //spdlog::trace(__VA_ARGS__)
	#define STORYT_INFO(...) //spdlog::info(__VA_ARGS__)
	#define STORYT_WARN(...) //spdlog::warn(__VA_ARGS__)
	#define STORYT_ERROR(...) spdlog::error(__VA_ARGS__)
	#define STORTY_CRITICAL(...) spdlog::critical(__VA_ARGS__)
	#define STORYT_WARNIF(cond, ...) //if(cond) spdlog::warn(__VA_ARGS__)
	#define STORYT_ERRORIF(cond, ...) if(cond) spdlog::error(__VA_ARGS__)
#else
	#define STORYT_TRACE(...)
	#define STORYT_INFO(...)
	#define STORYT_WARN(...)
	#define STORYT_ERROR(...)
	#define STORTY_CRITICAL(...)

	#define STORYT_WARNIF(cond, ...)
	#define STORYT_ERRORIF(cond, ...)
#endif

#ifdef NDEBUG
	#define STORYT_ASSERT(cond, ...) STORYT_ERRORIF(!cond, __VA_ARGS__)
#else
	#define STORYT_ASSERT(cond, ...) assert(cond)
#endif

#define STORYT_VERIFY(cond, ...) if(!cond) throw std::runtime_error("")

#ifndef STORYT_COMMON_H
#define STORYT_COMMON_H

namespace storyt::_internal
{

}

#endif // end of STORYT_COMMON_H