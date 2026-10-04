#include "ndef.hpp"

namespace boubox {

namespace {

constexpr uint8_t kTlvNull = 0x00;
constexpr uint8_t kTlvLockControl = 0x01;
constexpr uint8_t kTlvMemoryControl = 0x02;
constexpr uint8_t kTlvNdef = 0x03;
constexpr uint8_t kTlvTerminator = 0xFE;

constexpr uint8_t kFlagSr = 0x10;
constexpr uint8_t kFlagIl = 0x08;
constexpr uint8_t kTnfMask = 0x07;
constexpr uint8_t kTnfWellKnown = 0x01;

std::optional<std::string> ParseTextRecords(const uint8_t* msg, size_t len)
{
    size_t i = 0;
    while (i < len)
    {
        const uint8_t header = msg[i++];
        if (i >= len)
        {
            return std::nullopt;
        }
        const size_t type_len = msg[i++];

        size_t payload_len = 0;
        if (header & kFlagSr)
        {
            if (i >= len)
            {
                return std::nullopt;
            }
            payload_len = msg[i++];
        }
        else
        {
            if (i + 4 > len)
            {
                return std::nullopt;
            }
            payload_len = (static_cast<size_t>(msg[i]) << 24) | (static_cast<size_t>(msg[i + 1]) << 16) |
                          (static_cast<size_t>(msg[i + 2]) << 8) | msg[i + 3];
            i += 4;
        }

        size_t id_len = 0;
        if (header & kFlagIl)
        {
            if (i >= len)
            {
                return std::nullopt;
            }
            id_len = msg[i++];
        }

        if (type_len > len - i)
        {
            return std::nullopt;
        }
        const uint8_t* type = msg + i;
        i += type_len;
        if (id_len > len - i)
        {
            return std::nullopt;
        }
        i += id_len;
        if (payload_len > len - i)
        {
            return std::nullopt;
        }
        const uint8_t* payload = msg + i;
        i += payload_len;

        const bool is_text = (header & kTnfMask) == kTnfWellKnown && type_len == 1 && type[0] == 'T';
        if (is_text && payload_len >= 1)
        {
            const uint8_t status = payload[0];
            const size_t lang_len = status & 0x3F;
            const bool utf16 = (status & 0x80) != 0;
            if (!utf16 && 1 + lang_len <= payload_len)
            {
                return std::string(reinterpret_cast<const char*>(payload + 1 + lang_len),
                                   payload_len - 1 - lang_len);
            }
        }

        constexpr uint8_t kFlagMe = 0x40;
        if (header & kFlagMe)
        {
            break;
        }
    }
    return std::nullopt;
}

std::optional<std::string> ParseRawAscii(const uint8_t* data, size_t len)
{
    std::string text;
    for (size_t i = 0; i < len && data[i] != 0x00 && data[i] != 0xFF; ++i)
    {
        if (data[i] < 0x20 || data[i] > 0x7E)
        {
            return std::nullopt;
        }
        text.push_back(static_cast<char>(data[i]));
    }
    if (text.empty())
    {
        return std::nullopt;
    }
    return text;
}

}  // namespace

std::optional<std::string> ExtractTagText(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return std::nullopt;
    }

    const bool starts_with_tlv = data[0] == kTlvNull || data[0] == kTlvLockControl ||
                                 data[0] == kTlvMemoryControl || data[0] == kTlvNdef ||
                                 data[0] == kTlvTerminator;
    if (!starts_with_tlv)
    {
        return ParseRawAscii(data, len);
    }

    size_t i = 0;
    while (i < len)
    {
        const uint8_t tag = data[i++];
        if (tag == kTlvNull)
        {
            continue;
        }
        if (tag == kTlvTerminator || i >= len)
        {
            return std::nullopt;
        }

        size_t tlv_len = data[i++];
        if (tlv_len == 0xFF)
        {
            if (i + 2 > len)
            {
                return std::nullopt;
            }
            tlv_len = (static_cast<size_t>(data[i]) << 8) | data[i + 1];
            i += 2;
        }
        if (tlv_len > len - i)
        {
            return std::nullopt;
        }
        if (tag == kTlvNdef)
        {
            return ParseTextRecords(data + i, tlv_len);
        }
        i += tlv_len;
    }
    return std::nullopt;
}

}  // namespace boubox
