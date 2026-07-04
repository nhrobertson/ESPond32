#ifndef NETWORK_H
#define NETWORK_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mqtt_client.h"
#include "esp_sntp.h"

#define GOT_IP_BIT        BIT0
#define DISCONNECT_BIT    BIT1

static void task_net_manager(void *arg);

#endif //NETWORK_H
