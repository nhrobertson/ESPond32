idf_component_register(SRCS "device.c"
                       INCLUDE_DIRS "include"
                       REQUIRES io config led models esp_driver_gpio
)
