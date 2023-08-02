

#ifndef STORYT_PDF_COMPRESSION_H
#define STORYT_PDF_COMPRESSION_H

#include <array>
#include <cstring>
#include <zlib.h>

#include <storyt/common.h>

namespace storyt::_internal
{
	// https://github.com/mateidavid/zstr/blob/master/

	std::array<Bytef, 32> inflate(std::array<Bytef, 32> inbuff)
	{
		//std::array<Bytef, 16> inbuff = { 0x5D, 0xDB, 0x8E, 0xE4, 0x38, 0x72, 0x7D, 0xEF, 0xAF, 0xA8, 0x67, 0x03, 0xD6, 0xF0, 0x7E, 0x01 };
		std::array<Bytef, 32> outbuff{};
		z_streamp zstr = new z_stream;
		zstr->zalloc = Z_NULL;
		zstr->zfree = Z_NULL;
		zstr->opaque = Z_NULL;
		int ret = inflateInit2(zstr, 15 + 32);
		STORYT_ASSERT((ret == Z_OK), "Failed to Initialize inflate");

		Bytef* inbufferStart = inbuff.data();
		Bytef* inbufferEnd = inbuff.data() + inbuff.size();
		Bytef* outbufferStart = outbuff.data();
		Bytef* outbufferEnd = outbuff.data() + outbuff.size();

		while (outbufferStart != outbufferEnd)
		{
			zstr->next_in = inbufferStart;
			zstr->avail_in = (inbufferEnd - inbufferStart);
			zstr->next_out = outbufferStart;
			zstr->avail_out = (outbufferEnd - outbufferStart);

			ret = inflate(zstr, Z_NO_FLUSH);
			STORYT_ASSERT((ret == Z_OK || ret == Z_STREAM_END), "Failed to inflate data");

			if (ret == Z_STREAM_END)
			{
				break;
			}
		}
		inflateEnd(zstr);
		return outbuff;
	}

	std::string compressString(std::string& str, int compressionlevel = Z_BEST_COMPRESSION)
	{
		z_stream zs;
		memset(&zs, 0, sizeof(zs));
		int ret = deflateInit(&zs, compressionlevel);
		STORYT_ASSERT((ret == Z_OK), "Failed to init deflate");

		zs.next_in = reinterpret_cast<Bytef*>(str.data());
		zs.avail_in = str.size();
		char outbuffer[32768];
		std::string outstring;
		do {
			zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
			zs.avail_out = sizeof(outbuffer);

			ret = deflate(&zs, Z_FINISH);

			if (outstring.size() < zs.total_out) {
				outstring.append(outbuffer, zs.total_out - outstring.size());
			}
		} while (ret == Z_OK);
		deflateEnd(&zs);
		STORYT_ASSERT((ret == Z_OK || ret == Z_STREAM_END), "Unsuccessfuly compression");
		return outstring;
	}

	std::string decompressString(std::string& str)
	{
		z_stream zs;                        // z_stream is zlib's control structure
		memset(&zs, 0, sizeof(zs));
		inflateInit(&zs);
		zs.next_in = reinterpret_cast<Bytef*>(str.data());
		zs.avail_in = str.size();

		int ret;
		char outbuffer[32768];
		std::string outstring;
		do
		{
			zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
			zs.avail_out = sizeof(outbuffer);
			ret = inflate(&zs, 0);
			if (outstring.size() < zs.total_out)
			{
				outstring.append(outbuffer, zs.total_out - outstring.size());
			}
		} while (ret == Z_OK);
		inflateEnd(&zs);
		return outstring;
	}

	std::array<Bytef, 32> deflate()
	{
		std::array<Bytef, 5> inbuff = { 'H', 'e', 'l', 'l', 'o'};
		std::array<Bytef, 32> outbuff{};
		z_streamp zstr = new z_stream;
		zstr->zalloc = Z_NULL;
		zstr->zfree = Z_NULL;
		zstr->opaque = Z_NULL;
		/// The compression level must be Z_DEFAULT_COMPRESSION, or between 0 and 9:
		/// 1 gives best speed, 9 gives best compression, 0 gives no compression at all
		/// Z_DEFAULT_COMPRESSION requests a default compromise between speed and compression(currently
	    /// equivalent to level 6).
		int ret = deflateInit2(zstr, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
		STORYT_ASSERT((ret == Z_OK), "Failed to Initialize deflate");

		Bytef* inbufferStart = inbuff.data();
		Bytef* inbufferEnd = inbuff.data() + inbuff.size();
		Bytef* outbufferStart = outbuff.data();
		Bytef* outbufferEnd = outbuff.data() + outbuff.size();

		while (true)
		{
			zstr->next_in = inbufferStart;
			zstr->avail_in = (inbufferEnd - inbufferStart);
			zstr->next_out = outbufferStart;
			zstr->avail_out = (outbufferEnd - outbufferStart);
			ret = deflate(zstr, Z_FULL_FLUSH);
			STORYT_ASSERT((ret == Z_OK || ret == Z_STREAM_END || ret == Z_BUF_ERROR), "Failed to deflate");

			if (ret == Z_STREAM_END || ret == Z_BUF_ERROR) //|| (inbuff.size() - zstr->total_in) == 0)
			{
				break;
			}
			inbufferStart += zstr->total_in;
			outbufferStart += zstr->total_out;
		}
		ret = deflateEnd(zstr);
		//STORYT_ASSERT((ret == Z_OK || ret == Z_STREAM_END || ret == Z_BUF_ERROR), "Failed to deflate");
		return outbuff;
	}



} // End of storyt::_internal

#endif // End of STORYT_PDF_COMPRESSION_H