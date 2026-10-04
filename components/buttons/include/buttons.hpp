#pragma once

#include <array>
#include <functional>

#include "driver/gpio.h"
#include "esp_err.h"

namespace boubox {

enum class Button
{
    Up,
    Down,
    Next,
    Previous,
    Count
};

// Debounced push buttons wired between the GPIO and GND (internal pull-up used).
// Up/Down auto-repeat while held. The callback runs in the buttons task.
class Buttons
{
public:
    using Callback = std::function<void(Button)>;

    struct Config
    {
        std::array<gpio_num_t, static_cast<size_t>(Button::Count)> pins{};
        uint32_t debounce_ms = 30;
        uint32_t repeat_delay_ms = 500;
        uint32_t repeat_period_ms = 150;
    };

    esp_err_t Start(const Config& config, Callback callback);

private:
    struct State
    {
        bool pressed = false;
        uint32_t stable_ms = 0;
        int64_t pressed_at_ms = 0;
        int64_t last_repeat_ms = 0;
    };

    static void TaskEntry(void* arg);
    void Run();

    Config config_{};
    Callback callback_;
    std::array<State, static_cast<size_t>(Button::Count)> states_{};
};

}  // namespace boubox
