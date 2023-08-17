

#ifndef STORYT_COMMON_H
#define STORYT_COMMON_H

#if STORYT_BUILD_SPDLOG_ == true
#include <spdlog/spdlog.h>
#define STORYT_TRACE(...) spdlog::trace(__VA_ARGS__)
#define STORYT_INFO(...) spdlog::info(__VA_ARGS__)
#define STORYT_WARN(...) spdlog::warn(__VA_ARGS__)
#define STORYT_ERROR(...) spdlog::error(__VA_ARGS__)
#define STORTY_CRITICAL(...) spdlog::critical(__VA_ARGS__)
#define STORYT_WARNIF(cond, ...) if(cond) spdlog::warn(__VA_ARGS__)
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

#include <cstddef>
#include <fstream>
#include <array>
#include <vector>

namespace storyt::_internal
{
	using byte_t = uint8_t;
	using state_t = uint16_t;

	class File
	{
	public:
		explicit File(std::string path, int mode = std::ios::binary)
			:m_mode(mode), m_path(std::move(path)) {}
		~File()
		{
			if (m_file.is_open())
			{
				m_file.close();
			}
		}
		File& open()
		{
			m_file.open(m_path, m_mode);
			STORYT_ASSERT(m_file.is_open(), "Failed to open file [{}]", m_path);
			m_file.seekg(0, std::ios::end);
			m_fileSize = m_file.tellg();
			m_file.seekg(0);
			return *this;
		}
		[[nodiscard]] size_t getFileSize() const
		{
			return m_fileSize;
		}
		template<size_t OutBufferSize>
		std::array<byte_t, OutBufferSize> read(int startingPosition = -1)
		{
			std::array<byte_t, OutBufferSize> outbuffer(static_cast<byte_t>('\0'));
			if (startingPosition != -1)
			{
				m_file.seekg(startingPosition, std::ios::beg);
			}
			m_file.read(reinterpret_cast<char*>(outbuffer.data()), OutBufferSize);
			STORYT_ASSERT(m_file.fail(),
				"File [{}] failed to read [{}] bytes starting at [{}]", m_path, OutBufferSize, startingPosition);
			return outbuffer;
		}
		std::vector<byte_t> read(size_t outBufferSize, int startingPosition = -1)
		{
			std::vector<byte_t> outbuffer(outBufferSize, static_cast<byte_t>('\0'));
			if (startingPosition != -1)
			{
				m_file.seekg(startingPosition, std::ios::beg);
			}
			m_file.read(reinterpret_cast<char*>(outbuffer.data()), outBufferSize);
			STORYT_ASSERT(!m_file.fail(),
				"File [{}] failed to read [{}] bytes starting at [{}]. Is EOF [{}]", m_path, outBufferSize, startingPosition, m_file.eof());
			return outbuffer;
		}
	private:
		int m_mode;
		size_t m_fileSize{ 0 };
		std::string m_path;
		std::ifstream m_file;
	};
}

#endif // end of STORYT_COMMON_H