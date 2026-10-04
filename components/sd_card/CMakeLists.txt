idf_component_register(SRCS "sd_card.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_spi esp_driver_sdspi esp_driver_gpio fatfs sdmmc)
