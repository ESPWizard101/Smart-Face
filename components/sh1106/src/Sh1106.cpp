#include "Sh1106.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace drivers {
namespace {

constexpr const char* TAG = "sh1106";

/// Command set, see the SH1106 datasheet (Sino Wealth, rev 1.0), section "Commands".
namespace cmd {
constexpr uint8_t kSetColumnLow = 0x00;        // | low nibble of column
constexpr uint8_t kSetColumnHigh = 0x10;       // | high nibble of column
constexpr uint8_t kSetPumpVoltage8V = 0x32;
constexpr uint8_t kSetStartLine = 0x40;        // | line 0..63
constexpr uint8_t kSetContrast = 0x81;         // + 1 data byte
constexpr uint8_t kSegmentRemapNormal = 0xA0;
constexpr uint8_t kSegmentRemapFlipped = 0xA1;
constexpr uint8_t kEntireDisplayFromRam = 0xA4;
constexpr uint8_t kNormalDisplay = 0xA6;
constexpr uint8_t kInvertDisplay = 0xA7;
constexpr uint8_t kSetMultiplexRatio = 0xA8;   // + 1 data byte
constexpr uint8_t kDcDcControl = 0xAD;         // + 1 data byte
constexpr uint8_t kDcDcOn = 0x8B;
constexpr uint8_t kDisplayOff = 0xAE;
constexpr uint8_t kDisplayOn = 0xAF;
constexpr uint8_t kSetPage = 0xB0;             // | page 0..7
constexpr uint8_t kComScanIncrement = 0xC0;
constexpr uint8_t kComScanDecrement = 0xC8;
constexpr uint8_t kSetDisplayOffset = 0xD3;    // + 1 data byte
constexpr uint8_t kSetClockDivide = 0xD5;      // + 1 data byte
constexpr uint8_t kSetPrechargePeriod = 0xD9;  // + 1 data byte
constexpr uint8_t kSetComPins = 0xDA;          // + 1 data byte
constexpr uint8_t kSetVcomDeselect = 0xDB;     // + 1 data byte
}  // namespace cmd

constexpr uint32_t kResetPulseMs = 10;
constexpr uint32_t kPowerOnDelayMs = 100;  // VPP stabilisation after display on

}  // namespace

Sh1106::~Sh1106() {
    if (device_ != nullptr) {
        spi_bus_remove_device(device_);
    }
}

esp_err_t Sh1106::init(const Config& config) {
    ESP_RETURN_ON_FALSE(device_ == nullptr, ESP_ERR_INVALID_STATE, TAG, "already initialized");
    ESP_RETURN_ON_FALSE(config.dcPin != GPIO_NUM_NC, ESP_ERR_INVALID_ARG, TAG, "DC pin is required");

    dcPin_ = config.dcPin;
    resetPin_ = config.resetPin;
    columnOffset_ = config.columnOffset;

    uint64_t outputMask = 1ULL << dcPin_;
    if (resetPin_ != GPIO_NUM_NC) {
        outputMask |= 1ULL << resetPin_;
    }
    gpio_config_t io = {};
    io.pin_bit_mask = outputMask;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "GPIO config failed");

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.mode = 0;  // CPOL = 0, CPHA = 0
    deviceConfig.clock_speed_hz = config.clockHz;
    deviceConfig.spics_io_num = config.csPin;
    deviceConfig.queue_size = 1;
    ESP_RETURN_ON_ERROR(spi_bus_add_device(config.host, &deviceConfig, &device_), TAG, "spi_bus_add_device failed");

    hardwareReset();

    const uint8_t segmentRemap = config.rotate180 ? cmd::kSegmentRemapNormal : cmd::kSegmentRemapFlipped;
    const uint8_t comScan = config.rotate180 ? cmd::kComScanIncrement : cmd::kComScanDecrement;

    // clang-format off
    ESP_RETURN_ON_ERROR(sendCommands({
        cmd::kDisplayOff,
        cmd::kSetClockDivide, 0x80,          // default oscillator, divide ratio 1
        cmd::kSetMultiplexRatio, kHeight - 1,
        cmd::kSetDisplayOffset, 0x00,
        static_cast<uint8_t>(cmd::kSetStartLine | 0),
        cmd::kDcDcControl, cmd::kDcDcOn,     // built-in charge pump on
        cmd::kSetPumpVoltage8V,
        segmentRemap,
        comScan,
        cmd::kSetComPins, 0x12,              // alternative COM configuration (128x64)
        cmd::kSetContrast, config.contrast,
        cmd::kSetPrechargePeriod, 0x22,
        cmd::kSetVcomDeselect, 0x35,
        cmd::kEntireDisplayFromRam,
        cmd::kNormalDisplay,
    }), TAG, "init sequence failed");
    // clang-format on

    // Clear the RAM before turning the panel on so no power-up garbage is shown.
    // Deliberately not const: a zero-initialised array lives in DRAM (.bss), which the SPI DMA
    // can read directly, whereas a const one would be placed in flash.
    alignas(4) static uint8_t blankFrame[kFrameSize] = {};
    initialized_ = true;
    if (const esp_err_t err = writeFrame(blankFrame); err != ESP_OK) {
        initialized_ = false;
        return err;
    }

    ESP_RETURN_ON_ERROR(setDisplayOn(true), TAG, "display on failed");
    vTaskDelay(pdMS_TO_TICKS(kPowerOnDelayMs));

    ESP_LOGI(TAG, "ready: %dx%d, SPI %d kHz", kWidth, kHeight, config.clockHz / 1000);
    return ESP_OK;
}

esp_err_t Sh1106::writeFrame(std::span<const uint8_t, kFrameSize> frame) {
    ESP_RETURN_ON_FALSE(initialized_, ESP_ERR_INVALID_STATE, TAG, "not initialized");

    // The SH1106 has no horizontal addressing mode (unlike the SSD1306), so the frame is
    // sent page by page, setting the page and column address before each one.
    for (int page = 0; page < kPages; ++page) {
        ESP_RETURN_ON_ERROR(sendCommands({
                                static_cast<uint8_t>(cmd::kSetPage | page),
                                static_cast<uint8_t>(cmd::kSetColumnLow | (columnOffset_ & 0x0F)),
                                static_cast<uint8_t>(cmd::kSetColumnHigh | (columnOffset_ >> 4)),
                            }),
                            TAG, "page address failed");
        ESP_RETURN_ON_ERROR(transfer(frame.data() + page * kWidth, kWidth, true), TAG, "page data failed");
    }
    return ESP_OK;
}

esp_err_t Sh1106::setContrast(uint8_t contrast) {
    return sendCommands({cmd::kSetContrast, contrast});
}

esp_err_t Sh1106::setDisplayOn(bool on) {
    return sendCommands({on ? cmd::kDisplayOn : cmd::kDisplayOff});
}

esp_err_t Sh1106::setInverted(bool inverted) {
    return sendCommands({inverted ? cmd::kInvertDisplay : cmd::kNormalDisplay});
}

esp_err_t Sh1106::sendCommands(std::initializer_list<uint8_t> commands) {
    return transfer(commands.begin(), commands.size(), false);
}

esp_err_t Sh1106::transfer(const uint8_t* data, size_t length, bool isData) {
    ESP_RETURN_ON_FALSE(device_ != nullptr, ESP_ERR_INVALID_STATE, TAG, "not attached to a bus");
    if (length == 0) {
        return ESP_OK;
    }

    // Polling transfers are synchronous, so D/C can simply be set before each transaction.
    gpio_set_level(dcPin_, isData ? 1 : 0);

    spi_transaction_t transaction = {};
    transaction.length = length * 8;  // in bits
    transaction.tx_buffer = data;
    return spi_device_polling_transmit(device_, &transaction);
}

void Sh1106::hardwareReset() const {
    if (resetPin_ == GPIO_NUM_NC) {
        return;
    }
    gpio_set_level(resetPin_, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(resetPin_, 0);
    vTaskDelay(pdMS_TO_TICKS(kResetPulseMs));
    gpio_set_level(resetPin_, 1);
    vTaskDelay(pdMS_TO_TICKS(kResetPulseMs));
}

}  // namespace drivers
