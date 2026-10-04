#include "application.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    // Lives for the whole program lifetime, the tasks it starts keep running after app_main returns.
    static boubox::Application application;

    if (application.Start() != ESP_OK)
    {
        ESP_LOGE("boubox", "Startup failed, restarting in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
}
