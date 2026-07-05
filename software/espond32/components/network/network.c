#include "network.h"
#include "mqtt_client.h"
#include "portmacro.h"

static EventGroupHandle_t s_net_events;
static esp_mqtt_client_handle_t s_mqtt_client;
static bool s_mqtt_running = false;
static bool s_sntp_running = false;


wifi_config_t sta_cfg = {
  .sta = {
    .ssid       = "ssid",
    .password   = "password",
    .threshold.authmode = WIFI_AUTH_WPA2_PSK,
  }
};

esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address.uri = "mqtt://IP_ADDR",   // your Mosquitto box
    },
    .credentials = {
        .client_id = "espondcu",            // stable, human-readable
        .username  = "espond",                          // if the broker requires auth
        .authentication.password = "password",
    },
    .session = {
        .last_will = {
            .topic  = "pond/availability",
            .msg    = "offline",
            .qos    = 1,
            .retain = true,
        },
        .keepalive = 30,                              // seconds
    },
    .network = {
        .reconnect_timeout_ms = 10000,               // wait between auto-retries
        .timeout_ms           = 10000,               // network op timeout
    },  
};

static void start_mqtt(esp_mqtt_client_handle_t client) {
  if (s_mqtt_running) return;
  esp_mqtt_client_start(client);
  s_mqtt_running = true;
}

static void start_sntp(void) {
  if (s_sntp_running) return;
  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config); //Auto Syncs Time
  s_sntp_running = true;
}

static void stop_mqtt(esp_mqtt_client_handle_t client) {
  if (!s_mqtt_running) return;
  esp_mqtt_client_stop(client);
  s_mqtt_running = false;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupSetBits(s_net_events, DISCONNECT_BIT);
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_net_events, GOT_IP_BIT);
  }
}

static void register_network_handlers(void) {
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
      &wifi_event_handler, NULL, NULL);

  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
      &wifi_event_handler, NULL, NULL);
}

static void handle_config_msg(esp_mqtt_event_handle_t event) {
  char *temp_json = malloc(event->data_len + 1);
  if (temp_json == NULL) return;
  memcpy(temp_json, event->data, event->data_len);
  temp_json[event->data_len] = '\0';

  espond_cfg_t recieved;
  esp_err_t err = parse_config_json(temp_json, &recieved);
  free(temp_json);

  if (err != ESP_OK) return;

  if (recieved.version > g_espond_cfg.version) {
    xSemaphoreTake(cfg_buff_mutex, portMAX_DELAY);
    //Begin Replacement
    g_buff_cfg = recieved;
    xSemaphoreGive(cfg_buff_mutex);
    xEventGroupSetBits(g_events, CFG_CHANGED_BIT);
  }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      esp_mqtt_client_publish(event->client, "pond/availability", "online", 0, 1, true);

      esp_mqtt_client_subscribe(event->client, "pond/config", 1);
      esp_mqtt_client_subscribe(event->client, "pond/cmd", 1);
      break;
    case MQTT_EVENT_DATA:
      if (strncmp(event->topic, "pond/config", event->topic_len) == 0) {
        //Handle Config Message
        handle_config_msg(event);
      } else if (strncmp(event->topic, "pond/cmd", event->topic_len) == 0) {
        //TODO:
        //Handle Command Message
      }
      break;
    case MQTT_EVENT_ERROR:
      //TODO: Handle Error such as disconnect
      break;
    default:
      break;
  }
}


static void task_net_manager(void *arg) {
  s_net_events = xEventGroupCreate();

  //Begin network initalization
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  register_network_handlers();

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
  esp_wifi_start();

  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

  bool services_up = false;
  int backoff_ms = 1000;
  while (1) {
    EventBits_t bits = xEventGroupWaitBits(s_net_events, GOT_IP_BIT | DISCONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (bits & GOT_IP_BIT) {
      backoff_ms = 1000;
      start_sntp();
      start_mqtt(s_mqtt_client);
      services_up = true;
    }
    if (bits & DISCONNECT_BIT) {
      if (services_up) {
        stop_mqtt(s_mqtt_client);
        services_up = false;
      }
      vTaskDelay(pdMS_TO_TICKS(backoff_ms));
      if (backoff_ms < 30000) backoff_ms *= 2;
      esp_wifi_connect();
    }
  }
}
