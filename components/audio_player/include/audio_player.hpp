#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "wav.hpp"

namespace boubox {

// Plays all the WAV files (16-bit PCM, mono or stereo) of a folder through I2S.
// Public methods are thread safe: they only post commands to the player task.
class AudioPlayer
{
public:
    struct Config
    {
        i2s_port_t port = I2S_NUM_0;
        gpio_num_t bclk = GPIO_NUM_NC;
        gpio_num_t ws = GPIO_NUM_NC;
        gpio_num_t dout = GPIO_NUM_NC;
        gpio_num_t mclk = GPIO_NUM_NC;
        const char* base_path = "/sdcard";
        uint8_t initial_volume = 30;  // 0..100
        uint8_t volume_step = 5;
        uint32_t restart_threshold_ms = 3000;  // "Previous" restarts the track after this much playback
    };

    AudioPlayer() = default;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    esp_err_t Init(const Config& config);

    // Plays the WAV files found in <base_path>/<folder>, sorted by name.
    esp_err_t PlayFolder(const char* folder);
    void Next();
    void Previous();
    void Stop();
    void VolumeUp();
    void VolumeDown();
    uint8_t volume() const { return volume_.load(); }

    static bool IsValidFolderName(const char* name);

private:
    enum class CommandType : uint8_t
    {
        PlayFolder,
        Next,
        Previous,
        Stop
    };

    static constexpr size_t kMaxFolderLen = 64;
    struct Command
    {
        CommandType type;
        char folder[kMaxFolderLen + 1];
    };

    static constexpr size_t kReadBytes = 4096;

    static void TaskEntry(void* arg);
    void Run();
    void Post(CommandType type, const char* folder = nullptr);
    void HandleCommand(const Command& cmd);
    void SetVolume(int volume);

    bool LoadPlaylist(const char* folder);
    // Opens the first playable track from `index`, moving by `step` on failure.
    bool StartTrack(int index, int step);
    bool OpenTrack(const std::string& path);
    void CloseTrack();
    void StopPlayback();
    bool StreamChunk();
    esp_err_t PrepareOutput(uint32_t sample_rate);

    Config config_{};
    QueueHandle_t queue_ = nullptr;
    i2s_chan_handle_t tx_ = nullptr;
    bool channel_enabled_ = false;
    uint32_t current_rate_ = 0;
    std::atomic<uint8_t> volume_{0};

    std::vector<std::string> playlist_;
    int index_ = -1;
    std::FILE* file_ = nullptr;
    WavInfo wav_{};
    uint32_t bytes_remaining_ = 0;
    uint64_t frames_played_ = 0;

    int16_t read_buf_[kReadBytes / sizeof(int16_t)];
    int16_t out_buf_[kReadBytes];  // mono input is expanded to stereo
};

}  // namespace boubox
