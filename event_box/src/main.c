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
#include <string.h>

static void publish(const char *topic, const char *fmt, ...)
{
  char message[200];
  struct json_out json_message = JSON_OUT_BUF(message, sizeof(message));
  va_list ap;
  va_start(ap, fmt);
  int n = json_vprintf(&json_message, fmt, ap);
  va_end(ap);
  LOG(LL_INFO, (message));
  mgos_mqtt_pub(topic, message, n, 1, 0);
}

static void timer_cb(void *arg)
{
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long)mgos_get_heap_size(),
       (unsigned long)mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
#ifdef LED_PIN
  mgos_gpio_toggle(LED_PIN);
#endif
  (void)arg;

  int read = mgos_gpio_read(mgos_sys_config_get_pins_redButton());

  LOG(LL_INFO, ("RED BUTTON %d", read));
}

static void button_cb(int pin, void *arg)
{
  int led_pin;
  char button[100];
  if (pin == mgos_sys_config_get_pins_redButton())
  {
    led_pin = mgos_sys_config_get_pins_redLED();
    strncpy(button, "negative", sizeof(button));
  }
  else if (pin == mgos_sys_config_get_pins_greenButton())
  {
    led_pin = mgos_sys_config_get_pins_greenLED();
    strncpy(button, "positive", sizeof(button));
  }
  else
  {
    led_pin = mgos_sys_config_get_pins_blueLED();
    strncpy(button, "neutral", sizeof(button));
  }
  strncat(button, "_count", sizeof(button) - strlen(button) - 1);
  mgos_gpio_toggle(led_pin);

  char *format_str = "{ pathParameters: {type: EventBox }, body: {id: %Q, %Q: %d }}";
  publish("iot/EventBox", format_str, button, button, 1);

  LOG(LL_INFO, ("PRESSED BUTTON %s with pin  %d  and led_pin %d", button, pin, led_pin));
  LOG(LL_INFO, ("fmt: %s", format_str));
}

enum mgos_app_init_result mgos_app_init(void)
{
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif

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

  return MGOS_APP_INIT_SUCCESS;
}