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
#include "mgos_event.h"

#include <inttypes.h>
#include <string.h>

#include "pending.h"
#include <time.h>

static const char *s_our_ip = "192.168.4.1";
static const char *topic = "iot/EventBox";
static const char *old_format_str = "{ pathParameters: {type: EventBox },"
                                    "body: {id: %d, time: %d, positive_count: %d,"
                                    "neutral_count: %d, negative_count: %d }}";

static const char *format_str = "{ pathParameters: {type: EventBox },"
                                "body: {id: %d, time: %d, vote: %Q}}";

uint32_t id = 0;
char *event_id = NULL;
bool blinking = false;
bool time_is_known = false;
struct mgos_rlock_type *lock;

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

static uint16_t publish_vote(enum VoteType vote, uint32_t timestamp)
{
  char *vote_str;
  switch (vote)
  {
  case positive:
    vote_str = "positive";
    break;
  case neutral:
    vote_str = "neutral";
    break;
  case negative:
    vote_str = "negative";
    break;
  default:
    vote_str = "unknown";
    break;
  }

  return publish(topic, format_str, id, timestamp, vote_str);
}

static void handle_unsent_votes()
{
  mgos_rlock(lock);
  LOG(LL_INFO, ("handle unsent num_pending %d", num_pending_votes));

  int i = 0;
  uint16_t packet_id = 0;
  while (i < num_pending_votes)
  {
    packet_id = publish_vote(votes[i].vote, votes[i].timestamp);
    if (packet_id == 0)
      break;
    i++;
  }

  clear_pending_votes(i);
  write_pending_votes();

  mgos_runlock(lock);
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

  if (unsent_votes() && connected)
  {
    handle_unsent_votes();
  }

  (void)arg;
}

static void button_blink_cb(void *arg)
{
  int blue_led = mgos_sys_config_get_pins_blueLED();
  mgos_gpio_write((int)arg, 0);
  mgos_gpio_write(blue_led, 1);
}

static void blink_once(int pin)
{
  mgos_set_timer(300 /* ms */, 0, button_blink_cb, (void *)pin);

  int blue_led = mgos_sys_config_get_pins_blueLED();
  mgos_gpio_write(pin, 1);
  mgos_gpio_write(blue_led, 0);
}

static void button_cb(int pin, void *arg)
{
  if (!time_is_known)
  {
    blink_once(mgos_sys_config_get_pins_redLED());
    return;
  }
  blink_once(mgos_sys_config_get_pins_greenLED());

  enum VoteType vote;
  if (pin == mgos_sys_config_get_pins_redButton())
  {
    vote = negative;
  }
  else if (pin == mgos_sys_config_get_pins_greenButton())
  {
    vote = positive;
  }
  else
  {
    vote = neutral;
  }

  LOG(LL_INFO, ("time %d", (int)time(NULL)));

  uint16_t packet_id = publish_vote(vote, time(NULL));

  if (packet_id < 1)
  {
    mgos_rlock(lock);
    add_vote(vote, (int)time(NULL));
    mgos_runlock(lock);
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

static inline void time_change_cb(int ev, void *ev_data, void *userdata)
{
  double delta = *(double *)(ev_data);
  time_is_known = true;
  LOG(LL_INFO, ("time change Event: %lf", delta));
}

enum mgos_app_init_result mgos_app_init(void)
{
  ap_enabled(true);

  read_pending_votes();
  lock = mgos_rlock_create();

  init_led();
  init_buttons();
  init_id();
  LOG(LL_INFO, ("Event Box ID: %d", id));

  mgos_event_add_handler(MGOS_EVENT_TIME_CHANGED, time_change_cb, NULL);

  mgos_set_timer(1000 * 10 /* ms */, MGOS_TIMER_REPEAT | MGOS_TIMER_RUN_NOW, timer_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}