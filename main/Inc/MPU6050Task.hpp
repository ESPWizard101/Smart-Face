#pragma once
#include "freertos/FreeRTOS.h"

static constexpr uint8_t SDA = 3;
static constexpr uint8_t SCL = 3;
static constexpr uint16_t TaskStackDepth =3000;
TaskHandle_t 
constexpr BaseType_t TASK_PRIORITY = 4;
inline constexpr BaseType_t TASK_CORE = 1;

constexpr char TaskName[] ="OLED TASK";

class MPU6050Task {
    public:
        static void InitializeMPU6050Task();
        
        void StartMPU6050Task();
        void execute();
}