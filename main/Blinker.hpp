#pragma once

#include <cstdint>
#include "driver/gpio.h"

// Simple non-blocking-friendly LED blinker wrapping a GPIO output.
class Blinker
{
public:
    Blinker(gpio_num_t gpio, uint32_t periodMs);

    // Configures the GPIO as output. Must be called once before Toggle().
    void Init();

    // Flips the LED state. Call from a loop/timer at half the desired period.
    void Toggle();

    uint32_t PeriodMs() const { return periodMs_; }

private:
    gpio_num_t gpio_;
    uint32_t periodMs_;
    bool state_ = false;
};
