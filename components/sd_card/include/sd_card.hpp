#pragma once

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "sdmmc_cmd.h"

namespace boubox {

// SD card connected on a SPI bus, mounted as a FAT filesystem in the VFS.
class SdCard
{
public:
    struct Config
    {
        spi_host_device_t host = SPI2_HOST;
        gpio_num_t mosi = GPIO_NUM_NC;
        gpio_num_t miso = GPIO_NUM_NC;
        gpio_num_t sclk = GPIO_NUM_NC;
        gpio_num_t cs = GPIO_NUM_NC;
        int max_freq_khz = 20000;
        const char* mount_point = "/sdcard";
    };

    SdCard() = default;
    ~SdCard();
    SdCard(const SdCard&) = delete;
    SdCard& operator=(const SdCard&) = delete;

    esp_err_t Mount(const Config& config);
    void Unmount();
    bool mounted() const { return card_ != nullptr; }
    const char* mount_point() const { return mount_point_; }

private:
    sdmmc_card_t* card_ = nullptr;
    spi_host_device_t host_ = SPI2_HOST;
    const char* mount_point_ = "/sdcard";
};

}  // namespace boubox
