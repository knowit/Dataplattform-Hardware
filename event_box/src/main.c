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

#include <string.h>

bool blinking = false;
struct mg_str MAC;

struct pending_events
{
  uint16_t positive;
  uint16_t neutral;
  uint16_t negative;
  struct mgos_rlock_type *lock;
};

struct pending_events pending = {0};

static bool unsent_events(struct pending_events pending)
{
  return (pending.positive || pending.negative || pending.negative);
}

static void increase_unsent_events(uint16_t positive, uint16_t neutral, uint16_t negative)
{
  mgos_rlock(pending.lock);
  pending.positive += positive;
  pending.neutral += neutral;
  pending.negative += negative;
  mgos_runlock(pending.lock);
}

void clear_unsent_events()
{
  mgos_rlock(pending.lock);
  pending.positive = 0;
  pending.neutral = 0;
  pending.negative = 0;
  mgos_runlock(pending.lock);
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

static void timer_cb(void *arg)
{
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long)mgos_get_heap_size(),
       (unsigned long)mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
  (void)arg;

  LOG(LL_INFO, ("pending negative %d, neutral %d,  positive %d",
                pending.negative, pending.neutral, pending.positive));
  int read = mgos_gpio_read(mgos_sys_config_get_pins_redButton());

  bool connected = mgos_mqtt_global_is_connected();
  int blue_LED_pin = mgos_sys_config_get_pins_blueLED();
  LOG(LL_INFO, ("RED BUTTON %d blink %d, connect %d, blue %d", read, blinking, connected, mgos_gpio_read(blue_LED_pin)));
  if (connected && blinking)
  {
    LOG(LL_INFO, ("MQTT Connected"));
    mgos_gpio_blink(blue_LED_pin, 0, 0);
    mgos_gpio_write(blue_LED_pin, 1);
    blinking = false;
  }
  else if (!connected && (!blinking || !mgos_gpio_read(blue_LED_pin)))
  {
    LOG(LL_INFO, ("MQTT not connected"));
    mgos_gpio_blink(blue_LED_pin, 400, 400);
    blinking = true;
  }
  if (unsent_events(pending) && connected)
  {
    mgos_rlock(pending.lock);

    char *format_str = "{ pathParameters: {type: EventBox }, body: {id: %Q, positive_count: %d, neutral_count: %d, negative_count: %d }}";
    uint16_t packet_id = publish("iot/EventBox", format_str, MAC.p,
                                 pending.positive, pending.neutral, pending.negative);

    if (packet_id > 0)
    {
      pending.positive = 0;
      pending.neutral = 0;
      pending.negative = 0;
    }
    mgos_runlock(pending.lock);
  }
}

static void button_blink_cb(void *arg)
{
  int green_led = mgos_sys_config_get_pins_greenLED();
  int blue_led = mgos_sys_config_get_pins_blueLED();
  mgos_gpio_write(green_led, 0);
  mgos_gpio_write(blue_led, 1);
}

static void button_cb(int pin, void *arg)
{
  int positive = 0;
  int neutral = 0;
  int negative = 0;
  mgos_set_timer(300 /* ms */, 0, button_blink_cb, NULL);

  int green_led = mgos_sys_config_get_pins_greenLED();
  int blue_led = mgos_sys_config_get_pins_blueLED();
  mgos_gpio_write(green_led, 1);
  mgos_gpio_write(blue_led, 0);
  char button[100];
  if (pin == mgos_sys_config_get_pins_redButton())
  {
    //led_pin = mgos_sys_config_get_pins_redLED();
    strncpy(button, "negative", sizeof(button));
    negative++;
  }
  else if (pin == mgos_sys_config_get_pins_greenButton())
  {
    //led_pin = mgos_sys_config_get_pins_greenLED();
    strncpy(button, "positive", sizeof(button));
    positive++;
  }
  else
  {
    //led_pin = mgos_sys_config_get_pins_blueLED();
    strncpy(button, "neutral", sizeof(button));
    neutral++;
  }
  strncat(button, "_count", sizeof(button) - strlen(button) - 1);
  //mgos_gpio_toggle(led_pin);

  char *format_str = "{ pathParameters: {type: EventBox }, body: {id: %Q, positive_count: %d, neutral_count: %d, negative_count: %d }}";
  uint16_t packet_id = publish("iot/EventBox", format_str, MAC.p,
                               positive, neutral, negative);

  LOG(LL_INFO, ("PRESSED BUTTON %s with pin %d, packet_id %u", button, pin, packet_id));
  LOG(LL_INFO, ("fmt: %s", format_str));
  if (packet_id < 1)
  {
    increase_unsent_events(positive, neutral, negative);
  }
}

enum mgos_app_init_result mgos_app_init(void)
{
  MAC = mg_mk_str(mgos_sys_ro_vars_get_mac_address());
  pending.lock = mgos_rlock_create();

  int red_button_pin = mgos_sys_config_get_pins_redButton();
  int green_button_pin = mgos_sys_config_get_pins_greenButton();
  int yellow_button_pin = mgos_sys_config_get_pins_yellowButton();

  int red_LED_pin = mgos_sys_config_get_pins_redLED();
  int green_LED_pin = mgos_sys_config_get_pins_greenLED();
  int blue_LED_pin = mgos_sys_config_get_pins_blueLED();

  mgos_gpio_setup_output(red_LED_pin, 0);
  mgos_gpio_setup_output(green_LED_pin, 0);
  mgos_gpio_setup_output(blue_LED_pin, 0);

  int debounce_ms = 80;
  mgos_gpio_set_button_handler(red_button_pin, 0, 1, debounce_ms, button_cb, NULL);
  mgos_gpio_set_button_handler(green_button_pin, 0, 1, debounce_ms, button_cb, NULL);
  mgos_gpio_set_button_handler(yellow_button_pin, 0, 1, debounce_ms, button_cb, NULL);

  mgos_set_timer(1000 * 10 /* ms */, MGOS_TIMER_REPEAT | MGOS_TIMER_RUN_NOW, timer_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}