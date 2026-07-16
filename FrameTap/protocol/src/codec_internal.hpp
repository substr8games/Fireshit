#pragma once

#include "frametap/protocol/codec.hpp"

#include <limits>
#include <stdexcept>
#include <type_traits>

namespace frametap::protocol::detail
{
template<class T, bool = std::is_enum_v<T>>
struct RawType
{
    using type = T;
};

template<class T>
struct RawType<T, true>
{
    using type = std::underlying_type_t<T>;
};

template<class T>
using RawTypeT = typename RawType<T>::type;

class Writer
{
  public:
    template<class T>
    void value(T value)
    {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        using Raw = RawTypeT<T>;
        using Unsigned = std::make_unsigned_t<Raw>;
        const Unsigned raw = static_cast<Unsigned>(value);
        for (std::size_t i = 0; i < sizeof(Unsigned); ++i)
        {
            bytes.push_back(static_cast<std::uint8_t>(
              (raw >> (i * 8U)) & 0xffU));
        }
    }

    void string(const std::string& text)
    {
        if (text.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("string too large");
        }
        value<std::uint32_t>(static_cast<std::uint32_t>(text.size()));
        bytes.insert(bytes.end(), text.begin(), text.end());
    }

    std::vector<std::uint8_t> bytes;
};

class Reader
{
  public:
    explicit Reader(std::span<const std::uint8_t> input) : input(input) {}

    template<class T>
    T value()
    {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
        using Raw = RawTypeT<T>;
        using Unsigned = std::make_unsigned_t<Raw>;
        if (remaining() < sizeof(Unsigned))
        {
            throw std::runtime_error("truncated packet");
        }

        Unsigned raw = 0;
        for (std::size_t i = 0; i < sizeof(Unsigned); ++i)
        {
            raw |= static_cast<Unsigned>(input[offset++]) << (i * 8U);
        }
        return static_cast<T>(raw);
    }

    std::string string()
    {
        const auto length = value<std::uint32_t>();
        if (remaining() < length)
        {
            throw std::runtime_error("truncated string");
        }
        std::string result(
          reinterpret_cast<const char*>(input.data() + offset), length);
        offset += length;
        return result;
    }

    std::size_t remaining() const
    {
        return input.size() - offset;
    }

  private:
    std::span<const std::uint8_t> input;
    std::size_t offset = 0;
};

inline void write_header(Writer& writer, const Header& header)
{
    writer.value(header.magic_value);
    writer.value(header.version_value);
    writer.value(header.type);
    writer.value(header.size);
    writer.value(header.flags);
    writer.value(header.request_id);
    writer.value(header.session_id);
}

inline Header read_header(Reader& reader)
{
    Header header;
    header.magic_value = reader.value<std::uint32_t>();
    header.version_value = reader.value<std::uint16_t>();
    header.type = reader.value<std::uint16_t>();
    header.size = reader.value<std::uint32_t>();
    header.flags = reader.value<std::uint32_t>();
    header.request_id = reader.value<std::uint64_t>();
    header.session_id = reader.value<std::uint64_t>();
    return header;
}

inline void require_type(const Packet& packet, MessageType type)
{
    if (packet.header.type != static_cast<std::uint16_t>(type))
    {
        throw std::runtime_error("unexpected packet type");
    }
}
}
