#include <iostream>
#include <stdarg.h>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

#include "types.h"

#ifndef READER_UTILS_H
#define READER_UTILS_H


#define LOG(fmt, ...) reader::utils::log(__LINE__, fmt, __VA_ARGS__)
#define ASSERT(con, fmt, ...) do {\
                                if(!con)\
                                    LOG(fmt, __VA_ARGS__);\
                                    assert(con);\
                            }\
                         while(false);\

namespace reader {
	namespace utils {

        std::vector<int64_t> NID_TYPES_VALUES = {
                0x00,
                0x01,
                0x02,
                0x03,
                0x04,
                0x05,
                0x06,
                0x07,
                0x08,
                0x0A,
                0X0B,
                0x0C,
                0x0D,
                0x0E,
                0x0F,
                0x10,
                0x11,
                0x12,
                0x13,
                0x1F
        };

        std::vector<int64_t> BTYPE_VALUES = {
            0x6C,
            0x7C,
            0x8C,
            0x9C,
            0xA5,
            0xAC,
            0xB5,
            0xBC,
            0xCC,
        };

        void log(int lineNumber, const char* fmt, ...)
        {
            char buffer[1000]{};
            va_list vararglist;
            va_start(vararglist, fmt);
            vprintf(fmt, vararglist);
            va_end(vararglist);
            printf(" at line [%i]\n", lineNumber);
        }


        types::PType getPType(uint8_t ptype)
        {
            switch (ptype)
            {
            case 0x80:
                return types::PType::BBT;
            case 0x81:
                return types::PType::NBT;
            case 0x82:
                return types::PType::FMap;
            case 0x83:
                return types::PType::PMap;
            case 0x84:
                return types::PType::AMap;
            case 0x85:
                return types::PType::FPMap;
            case 0x86:
                return types::PType::DL;
            default:
                ASSERT(false, "[ERROR] Invalid PType %i", ptype);
                return types::PType::INVALID;
            }
        }

        std::string PTypeToString(types::PType t)
        {
            switch (t)
            {
            case types::PType::BBT:
                return "PType::BBT";
            case types::PType::NBT:
                return "PType::NBT";
            case types::PType::FMap:
                return "PType::FMap";
            case types::PType::PMap:
                return "PType::PMap";
            case types::PType::AMap:
                return "PType::AMap";
            case types::PType::FPMap:
                return "PType::FPMap";
            case types::PType::DL:
                return "PType::DL";
            default:
                return "Unknown PType";
            }
        }

        template<typename T>
        types::NIDType getNIDType(T t)
        {
            switch (t)
            {
            case 0x00:
                return types::NIDType::HID;
            case 0x01:
                return types::NIDType::INTERNAL;
            case 0x02:
                return types::NIDType::NORMAL_FOLDER;
            case 0x03:
                return types::NIDType::SEARCH_FOLDER;
            case 0x04:
                return types::NIDType::NORMAL_MESSAGE;
            case 0x05:
                return types::NIDType::ATTACHMENT;
            case 0x06:
                return types::NIDType::SEARCH_UPDATE_QUEUE;
            case 0x07:
                return types::NIDType::SEARCH_CRITERIA_OBJECT;
            case 0x08:
                return types::NIDType::ASSOC_MESSAGE;
            case 0x0A:
                return types::NIDType::CONTENTS_TABLE_INDEX;
            case 0X0B:
                return types::NIDType::RECEIVE_FOLDER_TABLE;
            case 0x0C:
                return types::NIDType::OUTGOING_QUEUE_TABLE;
            case 0x0D:
                return types::NIDType::HIERARCHY_TABLE;
            case 0x0E:
                return types::NIDType::CONTENTS_TABLE;
            case 0x0F:
                return types::NIDType::ASSOC_CONTENTS_TABLE;
            case 0x10:
                return types::NIDType::SEARCH_CONTENTS_TABLE;
            case 0x11:
                return types::NIDType::ATTACHMENT_TABLE;
            case 0x12:
                return types::NIDType::RECIPIENT_TABLE;
            case 0x13:
                return types::NIDType::SEARCH_TABLE_INDEX;
            case 0x1F:
                return types::NIDType::LTP;
            default:
                return types::NIDType::INVALID;
            }
        }

        template<typename T>
        std::string NIDTypeToString(T t)
        {
            switch ((int64_t)t)
            {
            case 0x00:
                return "NID_TYPE_HID";
            case 0x01:
                return "NID_TYPE_INTERNAL";
            case 0x02:
                return "NID_TYPE_NORMAL_FOLDER";
            case 0x03:
                return "NID_TYPE_SEARCH_FOLDER";
            case 0x04:
                return "NID_TYPE_NORMAL_MESSAGE";
            case 0x05:
                return "NID_TYPE_ATTACHMENT";
            case 0x06:
                return "NID_TYPE_SEARCH_UPDATE_QUEUE";
            case 0x07:
                return "NID_TYPE_SEARCH_CRITERIA_OBJECT";
            case 0x08:
                return "NID_TYPE_ASSOC_MESSAGE";
            case 0x0A:
                return "NID_TYPE_CONTENTS_TABLE_INDEX";
            case 0X0B:
                return "NID_TYPE_RECEIVE_FOLDER_TABLE";
            case 0x0C:
                return "NID_TYPE_OUTGOING_QUEUE_TABLE";
            case 0x0D:
                return "NID_TYPE_HIERARCHY_TABLE";
            case 0x0E:
                return "NID_TYPE_CONTENTS_TABLE";
            case 0x0F:
                return "NID_TYPE_ASSOC_CONTENTS_TABLE";
            case 0x10:
                return "NID_TYPE_SEARCH_CONTENTS_TABLE";
            case 0x11:
                return "NID_TYPE_ATTACHMENT_TABLE";
            case 0x12:
                return "NID_TYPE_RECIPIENT_TABLE";
            case 0x13:
                return "NID_TYPE_SEARCH_TABLE_INDEX";
            case 0x1F:
                return "NID_TYPE_LTP";
            default:
                return "Uknown NID Type";
            }
        }

        template<typename T>
        types::BType toBType(T t)
        {
            switch (t)
            {
            case 0x6C: return types::BType::Reserved1;
            case 0x7C: return types::BType::TC;
            case 0x8C: return types::BType::Reserved2;
            case 0x9C: return types::BType::Reserved3;
            case 0xA5: return types::BType::Reserved4;
            case 0xAC: return types::BType::Reserved5;
            case 0xB5: return types::BType::BTH;
            case 0xBC: return types::BType::PC;
            case 0xCC: return types::BType::Reserved6;
            default: return types::BType::Invalid;
            }
        }

        std::string toHex(const char byte)
        {
            static constexpr char HEXITS[] = "0123456789ABCDEF";
            std::string str(4, '\0');
            auto k = str.begin();
            *k++ = '0';
            *k++ = 'x';
            *k++ = HEXITS[byte >> 4];
            *k++ = HEXITS[byte & 0x0F];
            return str;
        }

        std::vector<std::string> toHexVector(const std::vector<types::byte_t>& bytes)
        {
            std::vector<std::string> hex(bytes.size());
            for (auto b : bytes)
            {
                hex.push_back(toHex(b));
            }
            return hex;
        }

        std::string toHexString(const std::vector<types::byte_t>& bytes, const char delimiter = ' ')
        {
            static constexpr char HEXITS[] = "0123456789ABCDEF";

            std::string str(5 * bytes.size(), '\0');
            auto k = str.begin();

            for (auto c : bytes) {
                *k++ = '0';
                *k++ = 'x';
                *k++ = HEXITS[c >> 4];
                *k++ = HEXITS[c & 0x0F];
                *k++ = delimiter;
            }
            return str;
        }

        std::vector<types::byte_t> pad(const std::vector<types::byte_t>& bytes, int64_t bytesToAdd)
        {
            std::vector<types::byte_t> result = bytes;
            if (bytesToAdd <= 0) return result;

            for (int64_t i = 0; i < bytesToAdd; i++)
            {
                result.push_back(0);
            }
            return result;
        }

        template<typename T = int>
        T toT_l(const std::vector<types::byte_t>& b)
        {
            auto bytes = pad(b, sizeof(T) - b.size());
            ASSERT((sizeof(T) == bytes.size()), "[ERROR] toT_l T not [%i].", sizeof(T));
            if constexpr (sizeof(T) == 1)
            {
                //static_assert(sizeof(T) == 1);
                return static_cast<T>
                    (
                        static_cast<T>(bytes[0])
                        );
            }
            else if constexpr (sizeof(T) == 2)
            {
                //static_assert(sizeof(T) == 2);
                return static_cast<T>
                    (
                        static_cast<T>(bytes[1]) << 8 |
                        static_cast<T>(bytes[0])
                        );
            }
            else if constexpr (sizeof(T) == 3)
            {
                //static_assert(sizeof(T) == 3);
                return static_cast<T>
                    (
                        static_cast<T>(bytes[2]) << 16 |
                        static_cast<T>(bytes[1]) << 8 |
                        static_cast<T>(bytes[0])
                        );
            }
            else if constexpr (sizeof(T) == 4)
            {
                //static_assert(sizeof(T) == 4);
                return static_cast<T>
                    (
                        static_cast<T>(bytes[3]) << 24 |
                        static_cast<T>(bytes[2]) << 16 |
                        static_cast<T>(bytes[1]) << 8 |
                        static_cast<T>(bytes[0])
                        );
            }
            else if constexpr (sizeof(T) == 8)
            {
                static_assert(sizeof(T) == 8);
                return static_cast<T>
                    (
                        static_cast<T>(bytes[7]) << 56 |
                        static_cast<T>(bytes[6]) << 48 |
                        static_cast<T>(bytes[5]) << 40 |
                        static_cast<T>(bytes[4]) << 32 |
                        static_cast<T>(bytes[3]) << 24 |
                        static_cast<T>(bytes[2]) << 16 |
                        static_cast<T>(bytes[1]) << 8 |
                        static_cast<T>(bytes[0])
                        );
            }
            ASSERT(false, "[ERROR] [bytes] &s vector can only be of size 2 to 4.", toHexString(bytes).c_str());
            return -1;
        }

        bool isEqual(std::vector<types::byte_t> a, std::vector<types::byte_t> b, std::string name = "NOT SET", bool shouldFail = true)
        {
            bool e = a == b;
            const char* msg = "[ERROR] [%s] [a] %s != [b] %s";

            if (shouldFail)
            {
                ASSERT(e, msg, name.c_str(), toHexString(a).c_str(), toHexString(b).c_str());
            }

            return e;
        }

        bool isIn(int64_t a, std::vector<int64_t> b)
        {
            bool found = false;
            for (auto item : b)
            {
                found |= item == a;
            }
            return found;
        }

        //template<typename T>
        std::vector<types::byte_t> toBits(int32_t integer, int64_t nbits)
		{
            ASSERT( (nbits > 0 && nbits <= 8 && nbits % 2 == 0), "[ERROR] nbits has to be between 1 and 8 and a multiple of 2");
            int64_t nElements = sizeof(integer) * (8 / nbits);
            int64_t mask = (int64_t)0xFF >> ((int64_t)8 - nbits);
			std::vector<types::byte_t> bytes(nElements);
			for (int64_t i = 0; i < nElements; i++)
			{
                // mask out the desired bits
                int64_t masked = integer & (mask << (i * nbits));
                // once masked, shift the bits to the right so they are in the first 8 bits
                int64_t shifted = masked >> (i * nbits);
                // if masked and shifted correctly, anding with the mask should be equal to shifted
                ASSERT(( (shifted & mask) == shifted), "[ERROR] shifted & mask != mask");
				bytes[i] = static_cast<types::byte_t>(shifted);
			}
			return bytes;
		}

        template<typename T>
        std::vector<types::byte_t> slice(const std::vector<types::byte_t>& v, T start, T end, T size)
        {
            ASSERT((end - start == size), "[ERROR] Invalid slice size [%i] != [%i]", end - start, size);
            return std::vector<types::byte_t>(v.begin() + start, v.begin() + end);
        }

        template<typename T, typename I>
        T slice(const std::vector<types::byte_t>& v, I start, I end, I size, T(*convert)(const std::vector<types::byte_t>&))
        {
            return convert(slice(v, start, end, size));
        }

        /**
         * @brief = Zero-indexed slice of bits from a byte vector
         * @param start = The inclusive start index in bits
         * @param end = The exclusive end index in bits
         * @param size = The size of the slice in bits
        */
        template<typename T, typename I>
        T sliceBits(const std::vector<types::byte_t>& bytes, I start, I end, I size)
        {
            ASSERT((end - start == size), "[ERROR] Invalid slice size [%i] != [%i]", end - start, size);
            ASSERT((size < 64), "[ERROR] Size must be less than 64 bits [%i]", size);
            int64_t k = toT_l<int64_t>(bytes);
            return static_cast<T>((k >> start) & ((1 << size) - 1));
        }

        template<typename R, typename T, typename I>
        R sliceBits(T integer, I start, I end, I size)
        {
            return sliceBits<R>(toBits(integer, 8), start, end, size);
        }

        template<typename T>
        std::vector<types::byte_t> readBytes(
            std::ifstream& file, 
            T numBytes)
        {
            std::vector<types::byte_t> res(numBytes, '\0');
            file.read((char*)res.data(), sizeof(types::byte_t) * res.size());
            return res;
        }

        template<class T>
        T readBytes(
            std::ifstream& file, 
            std::uint32_t numBytes, 
            T(*convert)(const std::vector<types::byte_t>&))
        {
            return convert(readBytes(file, numBytes));
        }
	}
}

#endif // !READER_UTILS_H