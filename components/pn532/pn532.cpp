#include "pn532.hpp"

#include <algorithm>
#include <cstring>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace boubox {

namespace {

constexpr char kTag[] = "pn532";

constexpr uint8_t kHostToPn532 = 0xD4;
constexpr uint8_t kPn532ToHost = 0xD5;

constexpr uint8_t kCmdGetFirmwareVersion = 0x02;
constexpr uint8_t kCmdSamConfiguration = 0x14;
constexpr uint8_t kCmdRfConfiguration = 0x32;
constexpr uint8_t kCmdInDataExchange = 0x40;
constexpr uint8_t kCmdInListPassiveTarget = 0x4A;

constexpr uint8_t kMifareRead = 0x30;
constexpr uint8_t kBaudRate106TypeA = 0x00;

constexpr uint32_t kI2cTimeoutMs = 100;
constexpr uint32_t kAckTimeoutMs = 100;
constexpr uint32_t kDefaultResponseTimeoutMs = 200;

// Framing overhead of a response read: status, 00 00 FF, LEN, LCS, TFI, CMD, DCS, postamble.
constexpr size_t kResponseOverhead = 10;

int64_t NowMs()
{
    return esp_timer_get_time() / 1000;
}

}  // namespace

Pn532::~Pn532()
{
    if (dev_ != nullptr)
    {
        i2c_master_bus_rm_device(dev_);
    }
    if (bus_ != nullptr)
    {
        i2c_del_master_bus(bus_);
    }
}

esp_err_t Pn532::Init(const Config& config)
{
    config_ = config;

    if (bus_ == nullptr)
    {
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.i2c_port = config_.port;
        bus_cfg.sda_io_num = config_.sda;
        bus_cfg.scl_io_num = config_.scl;
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = true;
        ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus_), kTag, "I2C bus creation failed");
    }

    if (dev_ == nullptr)
    {
        i2c_device_config_t dev_cfg = {};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = config_.address;
        dev_cfg.scl_speed_hz = config_.clock_hz;
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_, &dev_cfg, &dev_), kTag, "I2C device creation failed");
    }

    if (config_.reset != GPIO_NUM_NC)
    {
        gpio_config_t io_cfg = {};
        io_cfg.pin_bit_mask = 1ULL << config_.reset;
        io_cfg.mode = GPIO_MODE_OUTPUT;
        ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), kTag, "reset GPIO config failed");
    }

    return Restart();
}

esp_err_t Pn532::Restart()
{
    if (config_.reset != GPIO_NUM_NC)
    {
        gpio_set_level(config_.reset, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(config_.reset, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // The chip may be in power down: the first transactions can fail while it wakes up.
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        uint32_t version = 0;
        err = GetFirmwareVersion(&version);
        if (err == ESP_OK)
        {
            ESP_LOGI(kTag, "Found PN5%02X, firmware %u.%u", static_cast<unsigned>(version >> 24),
                     static_cast<unsigned>((version >> 16) & 0xFF), static_cast<unsigned>((version >> 8) & 0xFF));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_RETURN_ON_ERROR(err, kTag, "PN532 not responding");

    return Configure();
}

esp_err_t Pn532::Configure()
{
    // Normal mode, 1s virtual card timeout (unused), IRQ pin enabled.
    const uint8_t sam[] = {0x01, 0x14, 0x01};
    ESP_RETURN_ON_ERROR(Transact(kCmdSamConfiguration, sam, sizeof(sam), nullptr, 0, nullptr, kDefaultResponseTimeoutMs),
                        kTag, "SAMConfiguration failed");

    // MxRtyATR = 0xFF, MxRtyPSL = 0x01, MxRtyPassiveActivation = 0x02: finite retries so that
    // polling returns quickly when no tag is present.
    const uint8_t retries[] = {0x05, 0xFF, 0x01, 0x02};
    return Transact(kCmdRfConfiguration, retries, sizeof(retries), nullptr, 0, nullptr, kDefaultResponseTimeoutMs);
}

esp_err_t Pn532::GetFirmwareVersion(uint32_t* version)
{
    uint8_t resp[4];
    size_t resp_len = 0;
    ESP_RETURN_ON_ERROR(
        Transact(kCmdGetFirmwareVersion, nullptr, 0, resp, sizeof(resp), &resp_len, kDefaultResponseTimeoutMs), kTag,
        "GetFirmwareVersion failed");
    ESP_RETURN_ON_FALSE(resp_len == sizeof(resp), ESP_ERR_INVALID_RESPONSE, kTag, "bad firmware response");
    *version = (static_cast<uint32_t>(resp[0]) << 24) | (static_cast<uint32_t>(resp[1]) << 16) |
               (static_cast<uint32_t>(resp[2]) << 8) | resp[3];
    return ESP_OK;
}

esp_err_t Pn532::PollTarget(NfcTarget* target)
{
    const uint8_t args[] = {0x01, kBaudRate106TypeA};
    uint8_t resp[kMaxResponseData];
    size_t resp_len = 0;
    ESP_RETURN_ON_ERROR(
        Transact(kCmdInListPassiveTarget, args, sizeof(args), resp, sizeof(resp), &resp_len, kDefaultResponseTimeoutMs),
        kTag, "InListPassiveTarget failed");

    if (resp_len < 1 || resp[0] == 0)
    {
        return ESP_ERR_NOT_FOUND;
    }
    // NbTg, Tg, SENS_RES (2), SEL_RES, NFCIDLength, NFCID1...
    ESP_RETURN_ON_FALSE(resp_len >= 6, ESP_ERR_INVALID_RESPONSE, kTag, "short target response");
    const size_t uid_len = resp[5];
    ESP_RETURN_ON_FALSE(uid_len <= target->uid.size() && resp_len >= 6 + uid_len, ESP_ERR_INVALID_RESPONSE, kTag,
                        "bad UID length");

    *target = NfcTarget{};
    target->atqa = static_cast<uint16_t>((resp[2] << 8) | resp[3]);
    target->sak = resp[4];
    target->uid_len = static_cast<uint8_t>(uid_len);
    std::memcpy(target->uid.data(), resp + 6, uid_len);
    return ESP_OK;
}

esp_err_t Pn532::ReadPages(uint8_t first_page, uint8_t* out, size_t out_len)
{
    // A MIFARE READ returns 4 pages (16 bytes) at once.
    constexpr size_t kBlock = 16;
    for (size_t offset = 0; offset < out_len; offset += kBlock)
    {
        const uint8_t args[] = {0x01, kMifareRead, static_cast<uint8_t>(first_page + offset / 4)};
        uint8_t resp[1 + kBlock];
        size_t resp_len = 0;
        ESP_RETURN_ON_ERROR(Transact(kCmdInDataExchange, args, sizeof(args), resp, sizeof(resp), &resp_len,
                                     kDefaultResponseTimeoutMs),
                            kTag, "InDataExchange failed");
        ESP_RETURN_ON_FALSE(resp_len == sizeof(resp) && resp[0] == 0x00, ESP_FAIL, kTag, "page read refused (0x%02X)",
                            resp_len > 0 ? resp[0] : 0xFF);
        std::memcpy(out + offset, resp + 1, std::min(kBlock, out_len - offset));
    }
    return ESP_OK;
}

esp_err_t Pn532::Transact(uint8_t cmd, const uint8_t* data, size_t len, uint8_t* out, size_t out_cap,
                          size_t* out_len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(WriteCommand(cmd, data, len), kTag, "write failed");
    ESP_RETURN_ON_ERROR(ReadAck(kAckTimeoutMs), kTag, "no ACK");
    return ReadResponse(cmd, out, out_cap, out_len, timeout_ms);
}

esp_err_t Pn532::WriteCommand(uint8_t cmd, const uint8_t* data, size_t len)
{
    ESP_RETURN_ON_FALSE(len <= kMaxCommandData, ESP_ERR_INVALID_SIZE, kTag, "command too long");

    uint8_t frame[kMaxCommandData + 9];
    size_t n = 0;
    const uint8_t length = static_cast<uint8_t>(len + 2);  // TFI + command code + data

    frame[n++] = 0x00;
    frame[n++] = 0x00;
    frame[n++] = 0xFF;
    frame[n++] = length;
    frame[n++] = static_cast<uint8_t>(0x100 - length);
    frame[n++] = kHostToPn532;
    frame[n++] = cmd;
    uint8_t sum = static_cast<uint8_t>(kHostToPn532 + cmd);
    for (size_t i = 0; i < len; ++i)
    {
        frame[n++] = data[i];
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    frame[n++] = static_cast<uint8_t>(0x100 - sum);
    frame[n++] = 0x00;

    return i2c_master_transmit(dev_, frame, n, kI2cTimeoutMs);
}

esp_err_t Pn532::WaitReady(uint32_t timeout_ms)
{
    const int64_t deadline = NowMs() + timeout_ms;
    // The chip NACKs while busy: these expected NACKs would flood the log, so mute the driver while polling.
    const esp_log_level_t previous_level = esp_log_level_get("i2c.master");
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    esp_err_t result = ESP_ERR_TIMEOUT;
    do
    {
        uint8_t status = 0;
        if (i2c_master_receive(dev_, &status, 1, kI2cTimeoutMs) == ESP_OK && (status & 0x01))
        {
            result = ESP_OK;
            break;
        }
        vTaskDelay(1);
    } while (NowMs() < deadline);
    esp_log_level_set("i2c.master", previous_level);
    return result;
}

esp_err_t Pn532::ReadAck(uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(WaitReady(timeout_ms), kTag, "ACK timeout");

    static constexpr uint8_t kAckFrame[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    uint8_t rx[1 + sizeof(kAckFrame)];
    ESP_RETURN_ON_ERROR(i2c_master_receive(dev_, rx, sizeof(rx), kI2cTimeoutMs), kTag, "ACK read failed");
    return std::memcmp(rx + 1, kAckFrame, sizeof(kAckFrame)) == 0 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t Pn532::ReadResponse(uint8_t cmd, uint8_t* out, size_t out_cap, size_t* out_len, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(out_cap <= kMaxResponseData, ESP_ERR_INVALID_SIZE, kTag, "response buffer too large");
    ESP_RETURN_ON_ERROR(WaitReady(timeout_ms), kTag, "response timeout");

    uint8_t rx[kMaxResponseData + kResponseOverhead];
    const size_t rx_len = out_cap + kResponseOverhead;
    ESP_RETURN_ON_ERROR(i2c_master_receive(dev_, rx, rx_len, kI2cTimeoutMs), kTag, "response read failed");

    ESP_RETURN_ON_FALSE(rx[1] == 0x00 && rx[2] == 0x00 && rx[3] == 0xFF, ESP_ERR_INVALID_RESPONSE, kTag,
                        "bad preamble");
    const uint8_t length = rx[4];
    ESP_RETURN_ON_FALSE(static_cast<uint8_t>(length + rx[5]) == 0, ESP_ERR_INVALID_RESPONSE, kTag, "bad length checksum");
    // Length 1 + 0x7F is the application level error frame.
    ESP_RETURN_ON_FALSE(length >= 2, ESP_ERR_INVALID_RESPONSE, kTag, "error frame received");
    ESP_RETURN_ON_FALSE(static_cast<size_t>(length) + 7 <= rx_len, ESP_ERR_INVALID_SIZE, kTag, "response too long");

    const uint8_t* body = rx + 6;  // TFI, command code, data
    uint8_t sum = 0;
    for (size_t i = 0; i < static_cast<size_t>(length) + 1; ++i)
    {
        sum = static_cast<uint8_t>(sum + body[i]);
    }
    ESP_RETURN_ON_FALSE(sum == 0, ESP_ERR_INVALID_RESPONSE, kTag, "bad data checksum");
    ESP_RETURN_ON_FALSE(body[0] == kPn532ToHost && body[1] == cmd + 1, ESP_ERR_INVALID_RESPONSE, kTag,
                        "unexpected response 0x%02X", body[1]);

    const size_t data_len = length - 2;
    if (out != nullptr)
    {
        std::memcpy(out, body + 2, std::min(data_len, out_cap));
    }
    if (out_len != nullptr)
    {
        *out_len = data_len;
    }
    return ESP_OK;
}

}  // namespace boubox
