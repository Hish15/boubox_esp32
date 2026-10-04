#include "audio_player.hpp"

#include <algorithm>
#include <cstring>
#include <strings.h>

#include <dirent.h>

#include "esp_check.h"
#include "esp_log.h"

namespace boubox {

namespace {

constexpr char kTag[] = "audio";
constexpr uint32_t kWriteTimeoutMs = 1000;

bool HasWavExtension(const char* name)
{
    const size_t len = std::strlen(name);
    return len > 4 && strcasecmp(name + len - 4, ".wav") == 0;
}

}  // namespace

bool AudioPlayer::IsValidFolderName(const char* name)
{
    if (name == nullptr || name[0] == '\0' || std::strlen(name) > kMaxFolderLen)
    {
        return false;
    }
    if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
    {
        return false;
    }
    for (const char* c = name; *c != '\0'; ++c)
    {
        const auto ch = static_cast<unsigned char>(*c);
        if (ch < 0x20 || ch == 0x7F || *c == '/' || *c == '\\' || *c == ':')
        {
            return false;
        }
    }
    return true;
}

esp_err_t AudioPlayer::Init(const Config& config)
{
    ESP_RETURN_ON_FALSE(queue_ == nullptr, ESP_ERR_INVALID_STATE, kTag, "already initialised");
    config_ = config;
    volume_ = std::min<uint8_t>(config.initial_volume, 100);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(config_.port, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;  // output silence on underrun
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &tx_, nullptr), kTag, "I2S channel creation failed");

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = config_.mclk;
    std_cfg.gpio_cfg.bclk = config_.bclk;
    std_cfg.gpio_cfg.ws = config_.ws;
    std_cfg.gpio_cfg.dout = config_.dout;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx_, &std_cfg), kTag, "I2S init failed");
    current_rate_ = 44100;

    queue_ = xQueueCreate(8, sizeof(Command));
    ESP_RETURN_ON_FALSE(queue_ != nullptr, ESP_ERR_NO_MEM, kTag, "queue creation failed");
    ESP_RETURN_ON_FALSE(xTaskCreate(TaskEntry, "audio", 8192, this, 6, nullptr) == pdPASS, ESP_ERR_NO_MEM, kTag,
                        "task creation failed");
    return ESP_OK;
}

esp_err_t AudioPlayer::PlayFolder(const char* folder)
{
    ESP_RETURN_ON_FALSE(IsValidFolderName(folder), ESP_ERR_INVALID_ARG, kTag, "invalid folder name");
    Post(CommandType::PlayFolder, folder);
    return ESP_OK;
}

void AudioPlayer::Next()
{
    Post(CommandType::Next);
}

void AudioPlayer::Previous()
{
    Post(CommandType::Previous);
}

void AudioPlayer::Stop()
{
    Post(CommandType::Stop);
}

void AudioPlayer::VolumeUp()
{
    SetVolume(volume_.load() + config_.volume_step);
}

void AudioPlayer::VolumeDown()
{
    SetVolume(static_cast<int>(volume_.load()) - config_.volume_step);
}

void AudioPlayer::SetVolume(int volume)
{
    volume_ = static_cast<uint8_t>(std::clamp(volume, 0, 100));
    ESP_LOGI(kTag, "Volume: %u", static_cast<unsigned>(volume_.load()));
}

void AudioPlayer::Post(CommandType type, const char* folder)
{
    Command cmd = {};
    cmd.type = type;
    if (folder != nullptr)
    {
        std::strncpy(cmd.folder, folder, kMaxFolderLen);
    }
    if (xQueueSend(queue_, &cmd, 0) != pdTRUE)
    {
        ESP_LOGW(kTag, "Command queue full, command dropped");
    }
}

void AudioPlayer::TaskEntry(void* arg)
{
    static_cast<AudioPlayer*>(arg)->Run();
}

void AudioPlayer::Run()
{
    Command cmd;
    while (true)
    {
        if (file_ == nullptr)
        {
            xQueueReceive(queue_, &cmd, portMAX_DELAY);
            HandleCommand(cmd);
            continue;
        }

        while (xQueueReceive(queue_, &cmd, 0) == pdTRUE)
        {
            HandleCommand(cmd);
        }
        if (file_ != nullptr && !StreamChunk())
        {
            CloseTrack();
            StartTrack(index_ + 1, 1);
        }
    }
}

void AudioPlayer::HandleCommand(const Command& cmd)
{
    switch (cmd.type)
    {
    case CommandType::PlayFolder:
        StopPlayback();
        if (LoadPlaylist(cmd.folder))
        {
            StartTrack(0, 1);
        }
        break;

    case CommandType::Next:
        if (file_ != nullptr)
        {
            const int next = index_ + 1;
            CloseTrack();
            StartTrack(next, 1);
        }
        break;

    case CommandType::Previous:
        if (file_ != nullptr)
        {
            const uint64_t played_ms = frames_played_ * 1000 / wav_.sample_rate;
            if (played_ms > config_.restart_threshold_ms || index_ == 0)
            {
                std::fseek(file_, wav_.data_offset, SEEK_SET);
                bytes_remaining_ = wav_.data_size;
                frames_played_ = 0;
            }
            else
            {
                const int previous = index_ - 1;
                CloseTrack();
                StartTrack(previous, -1);
            }
        }
        break;

    case CommandType::Stop:
        StopPlayback();
        break;
    }
}

bool AudioPlayer::LoadPlaylist(const char* folder)
{
    const std::string dir_path = std::string(config_.base_path) + "/" + folder;
    DIR* dir = opendir(dir_path.c_str());
    if (dir == nullptr)
    {
        ESP_LOGW(kTag, "Folder '%s' not found", dir_path.c_str());
        return false;
    }

    playlist_.clear();
    while (const dirent* entry = readdir(dir))
    {
        if (entry->d_type == DT_DIR || entry->d_name[0] == '.' || !HasWavExtension(entry->d_name))
        {
            continue;
        }
        playlist_.push_back(dir_path + "/" + entry->d_name);
    }
    closedir(dir);

    std::sort(playlist_.begin(), playlist_.end(),
              [](const std::string& a, const std::string& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });

    if (playlist_.empty())
    {
        ESP_LOGW(kTag, "No WAV file in '%s'", dir_path.c_str());
        return false;
    }
    ESP_LOGI(kTag, "Playlist '%s': %u track(s)", folder, static_cast<unsigned>(playlist_.size()));
    return true;
}

bool AudioPlayer::StartTrack(int index, int step)
{
    while (index < static_cast<int>(playlist_.size()))
    {
        if (index < 0)
        {
            // Went before the first track: start over from the beginning.
            index = 0;
            step = 1;
        }
        if (OpenTrack(playlist_[index]))
        {
            index_ = index;
            return true;
        }
        index += step;
    }

    ESP_LOGI(kTag, "End of playlist");
    StopPlayback();
    return false;
}

bool AudioPlayer::OpenTrack(const std::string& path)
{
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        ESP_LOGW(kTag, "Cannot open %s", path.c_str());
        return false;
    }

    WavInfo info;
    if (!ParseWavHeader(file, &info) || info.bits_per_sample != 16 || info.channels < 1 || info.channels > 2 ||
        info.sample_rate == 0)
    {
        ESP_LOGW(kTag, "Unsupported WAV file %s", path.c_str());
        std::fclose(file);
        return false;
    }
    if (PrepareOutput(info.sample_rate) != ESP_OK)
    {
        std::fclose(file);
        return false;
    }

    file_ = file;
    wav_ = info;
    bytes_remaining_ = info.data_size;
    frames_played_ = 0;
    ESP_LOGI(kTag, "Playing %s (%u Hz, %u ch)", path.c_str(), static_cast<unsigned>(info.sample_rate),
             static_cast<unsigned>(info.channels));
    return true;
}

void AudioPlayer::CloseTrack()
{
    if (file_ != nullptr)
    {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void AudioPlayer::StopPlayback()
{
    CloseTrack();
    playlist_.clear();
    index_ = -1;

    if (channel_enabled_)
    {
        // Let the DMA buffers drain with silence before stopping the clocks.
        std::memset(out_buf_, 0, sizeof(out_buf_));
        size_t written = 0;
        i2s_channel_write(tx_, out_buf_, sizeof(out_buf_), &written, kWriteTimeoutMs);
        i2s_channel_disable(tx_);
        channel_enabled_ = false;
    }
}

esp_err_t AudioPlayer::PrepareOutput(uint32_t sample_rate)
{
    if (sample_rate != current_rate_)
    {
        if (channel_enabled_)
        {
            ESP_RETURN_ON_ERROR(i2s_channel_disable(tx_), kTag, "I2S disable failed");
            channel_enabled_ = false;
        }
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_clock(tx_, &clk_cfg), kTag, "I2S clock reconfig failed");
        current_rate_ = sample_rate;
    }
    if (!channel_enabled_)
    {
        ESP_RETURN_ON_ERROR(i2s_channel_enable(tx_), kTag, "I2S enable failed");
        channel_enabled_ = true;
    }
    return ESP_OK;
}

bool AudioPlayer::StreamChunk()
{
    const size_t frame_bytes = wav_.channels * sizeof(int16_t);
    const size_t to_read = std::min<size_t>(sizeof(read_buf_), bytes_remaining_) / frame_bytes * frame_bytes;
    if (to_read == 0)
    {
        return false;
    }

    const size_t read = std::fread(read_buf_, 1, to_read, file_);
    const size_t frames = read / frame_bytes;
    if (frames == 0)
    {
        return false;
    }
    bytes_remaining_ -= static_cast<uint32_t>(frames * frame_bytes);
    frames_played_ += frames;

    // Squared curve gives a more natural loudness progression. Q15 gain.
    const int32_t volume = volume_.load();
    const int32_t gain = volume * volume * 32768 / (100 * 100);

    const size_t samples_out = frames * 2;
    if (wav_.channels == 1)
    {
        for (size_t i = 0; i < frames; ++i)
        {
            const auto sample = static_cast<int16_t>((read_buf_[i] * gain) >> 15);
            out_buf_[2 * i] = sample;
            out_buf_[2 * i + 1] = sample;
        }
    }
    else
    {
        for (size_t i = 0; i < samples_out; ++i)
        {
            out_buf_[i] = static_cast<int16_t>((read_buf_[i] * gain) >> 15);
        }
    }

    size_t written = 0;
    const esp_err_t err = i2s_channel_write(tx_, out_buf_, samples_out * sizeof(int16_t), &written, kWriteTimeoutMs);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "I2S write failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

}  // namespace boubox
