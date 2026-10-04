#include "sd_card.hpp"

#include "driver/sdspi_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

namespace boubox {

namespace {
constexpr char kTag[] = "sd_card";
}

SdCard::~SdCard()
{
    Unmount();
}

esp_err_t SdCard::Mount(const Config& config)
{
    ESP_RETURN_ON_FALSE(card_ == nullptr, ESP_ERR_INVALID_STATE, kTag, "already mounted");

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config.mosi;
    bus_cfg.miso_io_num = config.miso;
    bus_cfg.sclk_io_num = config.sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4096;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(config.host, &bus_cfg, SPI_DMA_CH_AUTO), kTag, "SPI bus init failed");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = config.host;
    host.max_freq_khz = config.max_freq_khz;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = config.cs;
    slot_cfg.host_id = config.host;

    esp_vfs_fat_mount_config_t mount_cfg = {};
    mount_cfg.format_if_mount_failed = false;
    mount_cfg.max_files = 4;
    mount_cfg.allocation_unit_size = 16 * 1024;

    esp_err_t err = esp_vfs_fat_sdspi_mount(config.mount_point, &host, &slot_cfg, &mount_cfg, &card_);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Mount failed: %s", esp_err_to_name(err));
        card_ = nullptr;
        spi_bus_free(config.host);
        return err;
    }

    host_ = config.host;
    mount_point_ = config.mount_point;
    sdmmc_card_print_info(stdout, card_);
    return ESP_OK;
}

void SdCard::Unmount()
{
    if (card_ == nullptr)
    {
        return;
    }
    esp_vfs_fat_sdcard_unmount(mount_point_, card_);
    spi_bus_free(host_);
    card_ = nullptr;
}

}  // namespace boubox
