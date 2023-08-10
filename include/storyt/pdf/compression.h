

#ifndef STORYT_PDF_COMPRESSION_H
#define STORYT_PDF_COMPRESSION_H

#include <array>
#include <string>
#include <cstring>
#include <span>
#include <zlib.h>

#include <storyt/common.h>

namespace storyt::_internal
{
	std::string decompress(std::span<byte_t> inbuff)
	{
		z_stream zs;
		memset(&zs, 0, sizeof(zs));
		int ret = inflateInit(&zs);
		STORYT_ASSERT((ret == Z_OK), "Failed to init inflate");
		zs.next_in = reinterpret_cast<Bytef*>(inbuff.data());
		zs.avail_in = inbuff.size();

		char outbuffer[32768];
		std::string outstring;
		while(ret == Z_OK)
		{
			zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
			zs.avail_out = sizeof(outbuffer);
			ret = inflate(&zs, 0);
			if (outstring.size() < zs.total_out)
			{
				outstring.append(outbuffer, zs.total_out - outstring.size());
			}
		}
		inflateEnd(&zs);
		return outstring;
	}

	std::string compress(std::span<byte_t> inbuff, int compressionlevel = Z_BEST_COMPRESSION)
	{
		z_stream zs;
		memset(&zs, 0, sizeof(zs));
		/// The compression level must be Z_DEFAULT_COMPRESSION, or between 0 and 9:
		/// 1 gives best speed, 9 gives best compression, 0 gives no compression at all
		/// Z_DEFAULT_COMPRESSION requests a default compromise between speed and compression(currently
		/// equivalent to level 6).
		int ret = deflateInit(&zs, compressionlevel);
		STORYT_ASSERT((ret == Z_OK), "Failed to init deflate");

		zs.next_in = reinterpret_cast<Bytef*>(inbuff.data());
		zs.avail_in = inbuff.size();
		char outbuffer[32768];
		std::string outstring;
		while(ret == Z_OK)
		{
			zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
			zs.avail_out = sizeof(outbuffer);
			ret = deflate(&zs, Z_FINISH);
			if (outstring.size() < zs.total_out)
			{
				outstring.append(outbuffer, zs.total_out - outstring.size());
			}
		}
		deflateEnd(&zs);
		STORYT_ASSERT((ret == Z_OK || ret == Z_STREAM_END), "Unsuccessfuly compression");
		return outstring;
	}

} // End of storyt::_internal

#endif // End of STORYT_PDF_COMPRESSION_H