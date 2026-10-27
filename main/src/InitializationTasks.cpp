#include "InitializationTasks.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "MPU6050Task.hpp"
#include "OlEDTask.hpp"
#include "driver/spi_master.h"



esp_err_t InitializationTasks::InitializeTasks() {

    Initialize_SPI();
    OLEDTask::StartOLEDTask();

    Initialize_I2C();
    MPU6050Task::StartMPU6050Task();

    return ESP_OK; 
}

esp_err_t InitializationTasks::Initialize_SPI() {

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };
    esp_err_t ret = spi_bus_initialize(VSPI_HOST, &buscfg, 1);
    if (ret != ESP_OK) {
        ESP_LOGE("InitializationTasks", "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

esp_err_t InitializationTasks::Initialize_I2C() {

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU6050Task::SDA,
        .scl_io_num = MPU6050Task::SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE("InitializationTasks", "Failed to configure I2C parameters: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE("InitializationTasks", "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
