idf_component_register(SRCS "led.c"
                    INCLUDE_DIRS "include"
                    REQUIRES config models esp_driver_gpio led_strip
)
