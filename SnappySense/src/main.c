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
#include "mgos_adc.h"
#include "mgos_dht.h"

#include "co2_sensor.h"


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


static void handler(struct mg_connection *c, const char *topic, int topic_len, const char *msg, int msg_len, void *userdata){
	LOG(LL_INFO, ("Got message on topic %.*s", topic_len, topic));
}

static void timer_cb(void *arg) {
	struct mgos_dht *temp_humid;
	temp_humid = mgos_dht_create(mgos_sys_config_get_pins_temp(), AM2301);


	long int ppm = co2(mgos_sys_config_get_pins_co2());
	int light = 4095 - mgos_adc_read(mgos_sys_config_get_pins_photoresistor());
	float temp = mgos_dht_get_temp(temp_humid);
	float humidity = mgos_dht_get_humidity(temp_humid);
	int movement = mgos_gpio_read(mgos_sys_config_get_pins_pir());
	char id[20];
	strcpy(id, mgos_sys_config_get_thing_location());
	//char id[20] = mgos_sys_config_get_device_id();

	LOG(LL_INFO, (id));

	publish("iot/snappySense",
	        "{ pathParameters: {type: SnappySenseType }, body: {location: %Q, co2: %d, light: %d, temperature: %f, humidity: %f, movement: %d }}",
			id, ppm, light, temp, humidity, movement);


}

enum mgos_app_init_result mgos_app_init(void) {
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
  ap_enabled(true)
#endif
  //ap_enabled(true);
  mgos_gpio_setup_input(mgos_sys_config_get_pins_co2(), 0);
  mgos_gpio_setup_input(mgos_sys_config_get_pins_photoresistor(), 0);
  mgos_adc_enable(mgos_sys_config_get_pins_photoresistor());
  mgos_gpio_setup_input(mgos_sys_config_get_pins_temp(), 0);
  mgos_gpio_setup_input(mgos_sys_config_get_pins_pir(), 0);
  mgos_gpio_setup_output(mgos_sys_config_get_pins_ledr(), 0);
  mgos_gpio_setup_output(mgos_sys_config_get_pins_ledg(), 0);
  mgos_gpio_setup_output(mgos_sys_config_get_pins_ledb(), 0);
  mgos_gpio_write(mgos_sys_config_get_pins_ledr(), 1);
  mgos_gpio_write(mgos_sys_config_get_pins_ledg(), 1);
  mgos_gpio_write(mgos_sys_config_get_pins_ledb(), 1);
  mgos_set_timer(1000 * 10/* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
