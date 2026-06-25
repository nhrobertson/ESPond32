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

void app_main(void)
{

    //Check Flash Status
    
}
