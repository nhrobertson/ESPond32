#ifndef NETWORK_H
#define NETWORK_H

#include "esp_wifi.h"

#define GOT_IP_BIT        BIT0
#define DISCONNECT_BIT    BIT1

static void task_net_manager(void *arg);

#endif //NETWORK_H
