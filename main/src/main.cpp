#include "esp_log.h"

#include "App.hpp"

namespace {

constexpr const char* TAG = "main";

}  // namespace

extern "C" void app_main(void) {
    static smartface::App app;

    if (const esp_err_t err = app.start(); err != ESP_OK) {
        ESP_LOGE(TAG, "startup failed: %s", esp_err_to_name(err));
    }
}
