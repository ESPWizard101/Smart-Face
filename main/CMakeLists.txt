idf_component_register(
    SRCS
        "src/main.cpp"
        "src/App.cpp"
        "src/MotionTask.cpp"
        "src/MotionEstimator.cpp"
        "src/FaceTask.cpp"
        "src/Face.cpp"
        "src/Mood.cpp"
    INCLUDE_DIRS "Inc"
    PRIV_REQUIRES
        mpu6050
        sh1106
        gfx
        esp_driver_i2c
        esp_driver_spi
        esp_driver_gpio
        esp_timer
        esp_hw_support
        freertos
        log
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wshadow)
