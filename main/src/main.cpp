#include "esp_log.h"
#include <stdlib.h>
#include <cstdint>
#include "MPU6050Task.hpp"
#include "InitializationTasks.hpp"
#include "freertos/FreeRTOS.h"


extern "C" int app_main(void)
{
    ESP_LOGI("Main ","Program started");
    
    const esp_err_t result = InitializeTasks();
    if(result != ESP_OK) {
            ESP_LOGE("Main ","Failed to start task");
    }
}
