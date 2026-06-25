idf_component_register(SRCS "led.c"
                    INCLUDE_DIRS "include"
                    REQUIRES models esp_driver_gpio led_strip
)
