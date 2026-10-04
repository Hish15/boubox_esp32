#include "wav.hpp"

#include <cstring>

namespace boubox {

namespace {

constexpr uint16_t kFormatPcm = 0x0001;
constexpr uint16_t kFormatExtensible = 0xFFFE;

uint16_t Le16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t Le32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

bool ParseWavHeader(std::FILE* file, WavInfo* info)
{
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        return false;
    }
    const long file_size = std::ftell(file);
    if (file_size < 12 || std::fseek(file, 0, SEEK_SET) != 0)
    {
        return false;
    }

    uint8_t riff[12];
    if (std::fread(riff, 1, sizeof(riff), file) != sizeof(riff) || std::memcmp(riff, "RIFF", 4) != 0 ||
        std::memcmp(riff + 8, "WAVE", 4) != 0)
    {
        return false;
    }

    bool have_fmt = false;
    WavInfo result;
    long pos = 12;

    while (pos + 8 <= file_size)
    {
        uint8_t chunk[8];
        if (std::fseek(file, pos, SEEK_SET) != 0 || std::fread(chunk, 1, sizeof(chunk), file) != sizeof(chunk))
        {
            return false;
        }
        const uint32_t size = Le32(chunk + 4);
        const long body = pos + 8;

        if (std::memcmp(chunk, "fmt ", 4) == 0)
        {
            uint8_t fmt[40] = {};
            const size_t to_read = size < sizeof(fmt) ? size : sizeof(fmt);
            if (to_read < 16 || std::fread(fmt, 1, to_read, file) != to_read)
            {
                return false;
            }
            uint16_t format = Le16(fmt);
            if (format == kFormatExtensible)
            {
                // The sub format GUID starts with the actual format tag (offset 24).
                if (to_read < 26)
                {
                    return false;
                }
                format = Le16(fmt + 24);
            }
            if (format != kFormatPcm)
            {
                return false;
            }
            result.channels = Le16(fmt + 2);
            result.sample_rate = Le32(fmt + 4);
            result.bits_per_sample = Le16(fmt + 14);
            have_fmt = true;
        }
        else if (std::memcmp(chunk, "data", 4) == 0)
        {
            if (!have_fmt)
            {
                return false;
            }
            const long available = file_size - body;
            // Streamed files may carry a bogus size: clamp to what the file really holds.
            result.data_size = (size == 0 || size > static_cast<uint32_t>(available)) ? static_cast<uint32_t>(available) : size;
            result.data_offset = body;
            if (std::fseek(file, body, SEEK_SET) != 0)
            {
                return false;
            }
            *info = result;
            return true;
        }

        // Chunks are word aligned.
        pos = body + static_cast<long>(size) + static_cast<long>(size & 1);
    }
    return false;
}

}  // namespace boubox
