/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mgos.h"
#include "mgos_mqtt.h"
#include "mgos_ro_vars.h"

#include <inttypes.h>
#include <string.h>

#include "pending.h"

static const char *s_our_ip = "192.168.4.1";
static const char *format_str = "{ pathParameters: {type: EventBox },"
                                "body: {id: %d, event_id: %Q, positive_count: %d,"
                                "neutral_count: %d, negative_count: %d }}";
uint32_t id = 0;
char *event_id = NULL;
bool blinking = false;
struct pending_events pending = {0};

void ap_enabled(bool state)
{
  struct mgos_config_wifi_ap ap_cfg;
  memcpy(&ap_cfg, mgos_sys_config_get_wifi_ap(), sizeof(ap_cfg));
  ap_cfg.enable = state;
  if (!mgos_wifi_setup_ap(&ap_cfg))
  {
    LOG(LL_ERROR, ("Wifi AP setup failed"));
  }
}

static uint16_t
publish(const char *topic, const char *fmt, ...)
{
  char message[200];
  struct json_out json_message = JSON_OUT_BUF(message, sizeof(message));
  va_list ap;
  va_start(ap, fmt);
  int n = json_vprintf(&json_message, fmt, ap);
  va_end(ap);
  LOG(LL_INFO, (message));
  return mgos_mqtt_pub(topic, message, n, 1, 0);
}

static void handle_unsent_events()
{
  mgos_rlock(pending.lock);

  uint16_t packet_id = publish("iot/EventBox", format_str, id, pending.event_id,
                               pending.positive, pending.neutral, pending.negative);

  if (packet_id > 0)
  {
    pending.positive = 0;
    pending.neutral = 0;
    pending.negative = 0;
    write_pending(pending);
  }
  mgos_runlock(pending.lock);
}

static void sub_handler(struct mg_connection *nc, const char *topic,
                        int topic_len, const char *msg, int msg_len,
                        void *ud)
{
  LOG(LL_INFO, ("Handling the event  on topic %s  with len %d  with msg %s and len %d like a boss!",
                topic, topic_len, msg, msg_len));

  char *e_id = NULL;
  char *fmt = "{event_id: %Q}";
  json_scanf(msg, msg_len, fmt, &e_id);
  if (e_id)
  {
    LOG(LL_INFO, ("received event_id %s", e_id));
    /*
    We are Getting a new Event_id, so we must make sure there are no more pending 
    votes on the old event before we start accepting votes on the new event.
    */
    if (event_id == NULL || strcmp(event_id, e_id) == 0)
    {
      event_id = NULL;
      mgos_rlock(pending.lock);
      while (unsent_events(pending))
      {
        mgos_msleep(2000);
        handle_unsent_events();
      }
      free(event_id);
      event_id = e_id;
      strncpy(pending.event_id, event_id, sizeof(pending.event_id) - 1);
      mgos_runlock(pending.lock);
    }
    else
    {
      free(e_id);
    }
  }
}

static inline void init_id()
{
  uint64_t mac = 0;
  device_get_mac_address((uint8_t *)&mac);
  id = mac >> 32;
}

static void timer_cb(void *arg)
{
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long)mgos_get_heap_size(),
       (unsigned long)mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
  LOG(LL_INFO, ("pending negative %d, neutral %d,  positive %d",
                pending.negative, pending.neutral, pending.positive));

  bool connected = mgos_mqtt_global_is_connected();
  int blue_LED_pin = mgos_sys_config_get_pins_blueLED();
  if (connected && blinking)
  {
    ap_enabled(false);
    LOG(LL_INFO, ("MQTT Connected"));
    mgos_gpio_blink(blue_LED_pin, 0, 0);
    mgos_gpio_write(blue_LED_pin, 1);
    blinking = false;
  }
  else if (!connected && (!blinking || !mgos_gpio_read(blue_LED_pin)))
  {
    ap_enabled(true);
    LOG(LL_INFO, ("MQTT not connected"));
    mgos_gpio_blink(blue_LED_pin, 400, 400);
    blinking = true;
  }

  if (unsent_events(pending) && connected)
  {
    mgos_rlock(pending.lock);
    handle_unsent_events();
    mgos_runlock(pending.lock);
  }

  if (connected && !unsent_events(pending) && event_id == NULL)
  {
    publish("iot/EventIdRequest", "{ pathParameters: {type: EventBox },"
                                  "body: {event_code: can_haz_plz}}");
  }
  (void)arg;
}

static void button_blink_cb(void *arg)
{
  int green_led = mgos_sys_config_get_pins_greenLED();
  int blue_led = mgos_sys_config_get_pins_blueLED();
  //mgos_gpio_write(green_led, 0);
  mgos_gpio_write((int)arg, 0);
  mgos_gpio_write(blue_led, 1);
}

static void blink_once(int pin)
{
  mgos_set_timer(300 /* ms */, 0, button_blink_cb, (void *)pin);

  //int green_led = mgos_sys_config_get_pins_greenLED();
  int blue_led = mgos_sys_config_get_pins_blueLED();
  mgos_gpio_write(pin, 1);
  mgos_gpio_write(blue_led, 0);
}

static void button_cb(int pin, void *arg)
{
  if (event_id == NULL)
  {
    blink_once(mgos_sys_config_get_pins_redLED());
    return;
  }
  blink_once(mgos_sys_config_get_pins_greenLED());

  int positive = 0;
  int neutral = 0;
  int negative = 0;
  if (pin == mgos_sys_config_get_pins_redButton())
  {
    negative++;
  }
  else if (pin == mgos_sys_config_get_pins_greenButton())
  {
    positive++;
  }
  else
  {
    neutral++;
  }

  uint16_t packet_id = publish("iot/EventBox", format_str, id, event_id,
                               positive, neutral, negative);

  LOG(LL_INFO, (format_str, id, positive, neutral, negative));
  if (packet_id < 1)
  {
    increase_unsent_events(positive, neutral, negative, &pending);
  }
}

static inline void init_buttons()
{
  int red_button_pin = mgos_sys_config_get_pins_redButton();
  int green_button_pin = mgos_sys_config_get_pins_greenButton();
  int yellow_button_pin = mgos_sys_config_get_pins_yellowButton();

  int debounce_ms = 80;
  mgos_gpio_set_button_handler(red_button_pin, 0, 1, debounce_ms, button_cb, NULL);
  mgos_gpio_set_button_handler(green_button_pin, 0, 1, debounce_ms, button_cb, NULL);
  mgos_gpio_set_button_handler(yellow_button_pin, 0, 1, debounce_ms, button_cb, NULL);
}

static inline void init_led()
{
  int red_LED_pin = mgos_sys_config_get_pins_redLED();
  int green_LED_pin = mgos_sys_config_get_pins_greenLED();
  int blue_LED_pin = mgos_sys_config_get_pins_blueLED();

  mgos_gpio_setup_output(red_LED_pin, 0);
  mgos_gpio_setup_output(green_LED_pin, 0);
  mgos_gpio_setup_output(blue_LED_pin, 0);
}

enum mgos_app_init_result mgos_app_init(void)
{
  ap_enabled(true);

  read_pending(&pending);
  pending.lock = mgos_rlock_create();

  LOG(LL_INFO, ("pending negative %d, neutral %d,  positive %d",
                pending.negative, pending.neutral, pending.positive));

  init_led();
  init_buttons();
  init_id();
  LOG(LL_INFO, ("Event Box ID: %d", id));

  mgos_set_timer(1000 * 10 /* ms */, MGOS_TIMER_REPEAT | MGOS_TIMER_RUN_NOW, timer_cb, NULL);

  mgos_mqtt_sub("iot/EventId", sub_handler, NULL);

  return MGOS_APP_INIT_SUCCESS;
}