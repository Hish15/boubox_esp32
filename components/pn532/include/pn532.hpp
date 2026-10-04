#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace boubox {

struct NfcTarget
{
    std::array<uint8_t, 10> uid{};
    uint8_t uid_len = 0;
    uint16_t atqa = 0;
    uint8_t sak = 0;

    bool operator==(const NfcTarget& other) const
    {
        return uid_len == other.uid_len && uid == other.uid;
    }
};

// Minimal PN532 driver (I2C mode) able to detect ISO14443A targets and read
// NTAG2xx / Mifare Ultralight pages. Not thread safe.
class Pn532
{
public:
    struct Config
    {
        i2c_port_num_t port = I2C_NUM_0;
        gpio_num_t sda = GPIO_NUM_NC;
        gpio_num_t scl = GPIO_NUM_NC;
        gpio_num_t reset = GPIO_NUM_NC;  // optional
        uint32_t clock_hz = 100000;
        uint8_t address = 0x24;
    };

    Pn532() = default;
    ~Pn532();
    Pn532(const Pn532&) = delete;
    Pn532& operator=(const Pn532&) = delete;

    // Creates the I2C bus (first call only) and configures the chip.
    esp_err_t Init(const Config& config);
    // Resets and reconfigures the chip. Can be used to recover from errors.
    esp_err_t Restart();

    esp_err_t GetFirmwareVersion(uint32_t* version);
    // ESP_OK: target found, ESP_ERR_NOT_FOUND: no target in the field.
    esp_err_t PollTarget(NfcTarget* target);
    // Reads out_len bytes (rounded up to 4 byte pages) from first_page of the
    // currently selected target. out must hold a multiple of 16 bytes.
    esp_err_t ReadPages(uint8_t first_page, uint8_t* out, size_t out_len);

private:
    static constexpr size_t kMaxCommandData = 24;
    static constexpr size_t kMaxResponseData = 32;

    esp_err_t Configure();
    esp_err_t WriteCommand(uint8_t cmd, const uint8_t* data, size_t len);
    esp_err_t WaitReady(uint32_t timeout_ms);
    esp_err_t ReadAck(uint32_t timeout_ms);
    esp_err_t ReadResponse(uint8_t cmd, uint8_t* out, size_t out_cap, size_t* out_len, uint32_t timeout_ms);
    esp_err_t Transact(uint8_t cmd, const uint8_t* data, size_t len, uint8_t* out, size_t out_cap,
                       size_t* out_len, uint32_t timeout_ms);

    Config config_{};
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
};

}  // namespace boubox
