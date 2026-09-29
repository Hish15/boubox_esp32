#include "Blinker.hpp"

Blinker::Blinker(gpio_num_t gpio, uint32_t periodMs)
    : gpio_(gpio), periodMs_(periodMs)
{
}

void Blinker::Init()
{
    gpio_reset_pin(gpio_);
    gpio_set_direction(gpio_, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio_, state_);
}

void Blinker::Toggle()
{
    state_ = !state_;
    gpio_set_level(gpio_, state_);
}
