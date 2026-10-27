set(SOURCE
    src/main.cpp
    src/Display.cppS
    )   

idf_component_register(SRCS "main.cpp"
                        PRIV_REQUIRES driver log freertos esp_timer esp_system esp_app_format nvs_flash
                    INCLUDE_DIRS ".")
