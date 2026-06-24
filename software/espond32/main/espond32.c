/* ESPond32
  
   The ESPond32 is a IoT modular control unit to operate a series of pumps, a water valve, and monitoring water level

   It acts as the spirtual successor to the much less complex PondPi. This firmware is commited to the Public Domain for reuse under the GPL 3 license.
  
   Architectural Desicions:

   1. The main devices will all contain 3 GPIO Pins.
        - 2 Outputs to drive signals
        - 1 Input for configurable 3 state switch
   2. MQTT service to interact with a home server for Dashboard control instead of a on device hosted HTML webpage
   3. Drive signals take higher priority then Scheduabilty of said drive signals

   Hardware Requirements:

   The ESPond32 will have a custom pcb associated with this project, which preconnects input and output signals from the ESP32-WROOM-1

   For the inital version (v1), output signals to pumps and the valve will be driven by an external SSR, so a 3v3 gpio output signal is enough to statisfy
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "espond32";

#define BLINK_GPIO 38

static uint8_t s_led_state = 0;

#define CONFIG_BLINK_LED_GPIO 1
#define CONFIG_BLINK_PERIOD 1000


static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void app_main(void)
{

    /* Configure the peripheral according to the LED type */
    configure_led();

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
