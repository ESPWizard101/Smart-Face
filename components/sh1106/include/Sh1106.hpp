#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

namespace drivers {

class Sh1106 {
public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr int kPages = kHeight / 8;
    static constexpr size_t kFrameSize = static_cast<size_t>(kWidth) * kPages;

    struct Config {
        spi_host_device_t host = SPI3_HOST;
        gpio_num_t csPin = GPIO_NUM_NC;
        gpio_num_t dcPin = GPIO_NUM_NC;
        gpio_num_t resetPin = GPIO_NUM_NC;
        int clockHz = 8 * 1000 * 1000;
        uint8_t columnOffset = 2;
        bool rotate180 = false;
        uint8_t contrast = 0x80;
    };

    Sh1106() = default;
    ~Sh1106();

    Sh1106(const Sh1106&) = delete;
    Sh1106& operator=(const Sh1106&) = delete;

    esp_err_t init(const Config& config);

    esp_err_t writeFrame(std::span<const uint8_t, kFrameSize> frame);

    esp_err_t setContrast(uint8_t contrast);
    esp_err_t setDisplayOn(bool on);
    esp_err_t setInverted(bool inverted);

    bool isInitialized() const { return initialized_; }

private:
    esp_err_t sendCommands(std::initializer_list<uint8_t> commands);
    esp_err_t transfer(const uint8_t* data, size_t length, bool isData);
    void hardwareReset() const;

    spi_device_handle_t device_ = nullptr;
    gpio_num_t dcPin_ = GPIO_NUM_NC;
    gpio_num_t resetPin_ = GPIO_NUM_NC;
    uint8_t columnOffset_ = 2;
    bool initialized_ = false;
};

}  // namespace drivers
