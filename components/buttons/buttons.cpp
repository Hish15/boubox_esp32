#include "buttons.hpp"

#include <utility>

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace boubox {

namespace {

constexpr char kTag[] = "buttons";
constexpr uint32_t kScanPeriodMs = 10;

bool IsRepeatable(size_t index)
{
    return index == static_cast<size_t>(Button::Up) || index == static_cast<size_t>(Button::Down);
}

}  // namespace

esp_err_t Buttons::Start(const Config& config, Callback callback)
{
    config_ = config;
    callback_ = std::move(callback);

    for (gpio_num_t pin : config_.pins)
    {
        gpio_config_t io_cfg = {};
        io_cfg.pin_bit_mask = 1ULL << pin;
        io_cfg.mode = GPIO_MODE_INPUT;
        io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), kTag, "GPIO %d config failed", static_cast<int>(pin));
    }

    ESP_RETURN_ON_FALSE(xTaskCreate(TaskEntry, "buttons", 3072, this, 5, nullptr) == pdPASS, ESP_ERR_NO_MEM, kTag,
                        "task creation failed");
    return ESP_OK;
}

void Buttons::TaskEntry(void* arg)
{
    static_cast<Buttons*>(arg)->Run();
}

void Buttons::Run()
{
    TickType_t last_wake = xTaskGetTickCount();
    while (true)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kScanPeriodMs));
        const int64_t now_ms = esp_timer_get_time() / 1000;

        for (size_t i = 0; i < states_.size(); ++i)
        {
            State& state = states_[i];
            const bool raw_pressed = gpio_get_level(config_.pins[i]) == 0;

            if (raw_pressed == state.pressed)
            {
                state.stable_ms = 0;
                if (state.pressed && IsRepeatable(i) && now_ms - state.pressed_at_ms >= config_.repeat_delay_ms &&
                    now_ms - state.last_repeat_ms >= config_.repeat_period_ms)
                {
                    state.last_repeat_ms = now_ms;
                    callback_(static_cast<Button>(i));
                }
                continue;
            }

            state.stable_ms += kScanPeriodMs;
            if (state.stable_ms < config_.debounce_ms)
            {
                continue;
            }

            state.stable_ms = 0;
            state.pressed = raw_pressed;
            if (state.pressed)
            {
                state.pressed_at_ms = now_ms;
                state.last_repeat_ms = now_ms;
                callback_(static_cast<Button>(i));
            }
        }
    }
}

}  // namespace boubox
