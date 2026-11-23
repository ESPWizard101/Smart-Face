//#pragma once


class InitializationTasks {
    public:
        static esp_err_t Initialize_SPI();
        static esp_err_t Initialize_I2C();

        static esp_err_t InitializeTasks();
}