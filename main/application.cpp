#include "application.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace boubox {

namespace {
constexpr char kTag[] = "boubox";
}

esp_err_t Application::Start()
{
    SdCard::Config sd_cfg;
    sd_cfg.mosi = static_cast<gpio_num_t>(CONFIG_BOUBOX_SD_MOSI_GPIO);
    sd_cfg.miso = static_cast<gpio_num_t>(CONFIG_BOUBOX_SD_MISO_GPIO);
    sd_cfg.sclk = static_cast<gpio_num_t>(CONFIG_BOUBOX_SD_SCLK_GPIO);
    sd_cfg.cs = static_cast<gpio_num_t>(CONFIG_BOUBOX_SD_CS_GPIO);
    sd_cfg.max_freq_khz = CONFIG_BOUBOX_SD_MAX_FREQ_KHZ;
    ESP_RETURN_ON_ERROR(sd_card_.Mount(sd_cfg), kTag, "SD card mount failed");

    AudioPlayer::Config audio_cfg;
    audio_cfg.bclk = static_cast<gpio_num_t>(CONFIG_BOUBOX_I2S_BCLK_GPIO);
    audio_cfg.ws = static_cast<gpio_num_t>(CONFIG_BOUBOX_I2S_WS_GPIO);
    audio_cfg.dout = static_cast<gpio_num_t>(CONFIG_BOUBOX_I2S_DOUT_GPIO);
    audio_cfg.base_path = sd_card_.mount_point();
    audio_cfg.initial_volume = CONFIG_BOUBOX_DEFAULT_VOLUME;
    audio_cfg.volume_step = CONFIG_BOUBOX_VOLUME_STEP;
    ESP_RETURN_ON_ERROR(player_.Init(audio_cfg), kTag, "audio player init failed");

    Buttons::Config button_cfg;
    button_cfg.pins[static_cast<size_t>(Button::Up)] = static_cast<gpio_num_t>(CONFIG_BOUBOX_BUTTON_UP_GPIO);
    button_cfg.pins[static_cast<size_t>(Button::Down)] = static_cast<gpio_num_t>(CONFIG_BOUBOX_BUTTON_DOWN_GPIO);
    button_cfg.pins[static_cast<size_t>(Button::Next)] = static_cast<gpio_num_t>(CONFIG_BOUBOX_BUTTON_NEXT_GPIO);
    button_cfg.pins[static_cast<size_t>(Button::Previous)] =
        static_cast<gpio_num_t>(CONFIG_BOUBOX_BUTTON_PREVIOUS_GPIO);
    ESP_RETURN_ON_ERROR(buttons_.Start(button_cfg, [this](Button button) { OnButton(button); }), kTag,
                        "buttons init failed");

    Pn532::Config nfc_cfg;
    nfc_cfg.sda = static_cast<gpio_num_t>(CONFIG_BOUBOX_NFC_SDA_GPIO);
    nfc_cfg.scl = static_cast<gpio_num_t>(CONFIG_BOUBOX_NFC_SCL_GPIO);
    nfc_cfg.reset = static_cast<gpio_num_t>(CONFIG_BOUBOX_NFC_RESET_GPIO);
    ESP_RETURN_ON_ERROR(tag_scanner_.Start(nfc_cfg, CONFIG_BOUBOX_NFC_POLL_PERIOD_MS,
                                           [this](const std::string& folder) { OnTag(folder); }),
                        kTag, "tag scanner init failed");

    ESP_LOGI(kTag, "Ready");
    return ESP_OK;
}

void Application::OnButton(Button button)
{
    switch (button)
    {
    case Button::Up:
        player_.VolumeUp();
        break;
    case Button::Down:
        player_.VolumeDown();
        break;
    case Button::Next:
        player_.Next();
        break;
    case Button::Previous:
        player_.Previous();
        break;
    case Button::Count:
        break;
    }
}

void Application::OnTag(const std::string& folder)
{
    if (player_.PlayFolder(folder.c_str()) != ESP_OK)
    {
        ESP_LOGW(kTag, "Ignoring tag with invalid folder name '%s'", folder.c_str());
    }
}

}  // namespace boubox
