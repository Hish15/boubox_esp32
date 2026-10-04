#pragma once

#include <cstdint>
#include <cstdio>

namespace boubox {

struct WavInfo
{
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    long data_offset = 0;     // position of the first sample in the file
    uint32_t data_size = 0;   // size in bytes of the sample data
};

// Parses the header of a PCM WAV file (RIFF, 16-bit PCM or WAVE_FORMAT_EXTENSIBLE PCM).
// On success the file position is left at the start of the sample data.
bool ParseWavHeader(std::FILE* file, WavInfo* info);

}  // namespace boubox
