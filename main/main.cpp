#include "Blinker.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace
{
    constexpr char kTag[] = "blink";
}

extern "C" void app_main(void)
{
    Blinker blinker(static_cast<gpio_num_t>(CONFIG_BLINK_GPIO), CONFIG_BLINK_PERIOD_MS);
    blinker.Init();

    ESP_LOGI(kTag, "Blinking on GPIO%d every %lums", CONFIG_BLINK_GPIO, static_cast<unsigned long>(blinker.PeriodMs()));

    while (true)
    {
        blinker.Toggle();
        vTaskDelay(pdMS_TO_TICKS(blinker.PeriodMs() / 2));
        ESP_LOGI(kTag, "Toggled GPIO%d", CONFIG_BLINK_GPIO);
    
    }
}
