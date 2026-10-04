#include "tag_scanner.hpp"

#include <array>
#include <utility>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ndef.hpp"

namespace boubox {

namespace {

constexpr char kTag[] = "tag_scanner";

constexpr uint8_t kFirstUserPage = 4;
constexpr size_t kUserBytes = 64;
constexpr int kMissesBeforeRemoval = 3;
constexpr int kErrorsBeforeRestart = 5;
constexpr uint32_t kInitRetryMs = 2000;

}  // namespace

esp_err_t TagScanner::Start(const Pn532::Config& pn532_config, uint32_t poll_period_ms, Callback callback)
{
    pn532_config_ = pn532_config;
    poll_period_ms_ = poll_period_ms;
    callback_ = std::move(callback);
    ESP_RETURN_ON_FALSE(xTaskCreate(TaskEntry, "tag_scanner", 4096, this, 4, nullptr) == pdPASS, ESP_ERR_NO_MEM, kTag,
                        "task creation failed");
    return ESP_OK;
}

void TagScanner::TaskEntry(void* arg)
{
    static_cast<TagScanner*>(arg)->Run();
}

bool TagScanner::ReadText(std::string* text)
{
    std::array<uint8_t, kUserBytes> data{};
    if (pn532_.ReadPages(kFirstUserPage, data.data(), data.size()) != ESP_OK)
    {
        return false;
    }
    auto extracted = ExtractTagText(data.data(), data.size());
    if (!extracted)
    {
        ESP_LOGW(kTag, "Tag does not contain any text");
        return false;
    }
    *text = std::move(*extracted);
    return true;
}

void TagScanner::Run()
{
    while (pn532_.Init(pn532_config_) != ESP_OK)
    {
        ESP_LOGE(kTag, "PN532 initialisation failed, retrying");
        vTaskDelay(pdMS_TO_TICKS(kInitRetryMs));
    }
    ESP_LOGI(kTag, "Waiting for a tag");

    NfcTarget last_target;
    bool tag_present = false;
    bool tag_handled = false;
    int misses = 0;
    int errors = 0;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(poll_period_ms_));

        NfcTarget target;
        const esp_err_t err = pn532_.PollTarget(&target);

        if (err == ESP_ERR_NOT_FOUND)
        {
            errors = 0;
            if (tag_present && ++misses >= kMissesBeforeRemoval)
            {
                tag_present = false;
                tag_handled = false;
                ESP_LOGI(kTag, "Tag removed");
            }
            continue;
        }

        if (err != ESP_OK)
        {
            if (++errors >= kErrorsBeforeRestart)
            {
                ESP_LOGW(kTag, "Too many errors, restarting the PN532");
                errors = 0;
                pn532_.Restart();
            }
            continue;
        }

        errors = 0;
        misses = 0;
        if (!tag_present || !(target == last_target))
        {
            tag_present = true;
            tag_handled = false;
            last_target = target;
        }

        // Retried on each poll until the tag content has been read successfully.
        std::string text;
        if (!tag_handled && ReadText(&text))
        {
            tag_handled = true;
            ESP_LOGI(kTag, "Tag text: '%s'", text.c_str());
            callback_(text);
        }
    }
}

}  // namespace boubox
