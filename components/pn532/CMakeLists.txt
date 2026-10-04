idf_component_register(SRCS "pn532.cpp" "ndef.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2c esp_driver_gpio
                       PRIV_REQUIRES esp_timer)
