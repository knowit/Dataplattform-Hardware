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
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "mgos_mqtt.h"
#include "mgos_adc.h"
#include "mgos_dht.h"

#include "co2_sensor.h"

float temp_reading = 0;
float humidity_reading = 0;
float co2_reading = 0;
int mic_reading = 0;

static void publish_data(const char *topic, const char *fmt, ...)
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

static void read_co2(void *arg)
{
	LOG(LL_INFO, ("Starting co2 sampling"));
	int readCount = 0;
	int readSum = 0;
	while (readCount < 5)
	{
		long ppm = co2(mgos_sys_config_get_pins_co2());
		mgos_wdt_feed();
		if (ppm > 0)
		{
			readCount++;
			readSum += (int)ppm;
		}
		LOG(LL_INFO, ("PPM: %li %d", ppm, readCount));
	}
	co2_reading = readSum / readCount;
	LOG(LL_INFO, ("Avg PPM:  %f", co2_reading));
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_co2(), 0);
	publish_data("/snappySense",
				 "{co2: %f, temperature: %f, humidity: %f, mic: %d }",
				 co2_reading, temp_reading, humidity_reading, mic_reading);
}

static void warmup_co2()
{
	LOG(LL_INFO, ("Warmup co2"));
	// Warm up co2 sensor for at least 3 minutes to get good readings
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_co2(), 1);
	mgos_set_timer(1 * 60 * 1000 /* ms */, false, read_co2, NULL);
}

static void read_mic(void *arg)
{
	LOG(LL_INFO, ("Starting mic sampling"));
	int max = 0;
	for (int i = 0; i < 1000; i++)
	{
		int noise = mgos_adc_read(mgos_sys_config_get_pins_mic());
		if (noise > max)
		{
			max = noise;
		}
	}
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_mic(), 0);
	mic_reading = max;
	LOG(LL_INFO, ("Mic: %d", mic_reading));
	mgos_wdt_feed();

	warmup_co2();
}

static void warmup_mic()
{
	LOG(LL_INFO, ("Warmup mic"));
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_mic(), 1);
	mgos_set_timer(10 * 1000 /* ms */, false, read_mic, NULL);
}

static void read_dht(void *arg)
{
	LOG(LL_INFO, ("Starting DHT sampling"));
	struct mgos_dht *temp_humid;
	temp_humid = mgos_dht_create(mgos_sys_config_get_pins_temp(), AM2301);
	temp_reading = mgos_dht_get_temp(temp_humid);
	humidity_reading = mgos_dht_get_humidity(temp_humid);
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_dht(), 0);
	LOG(LL_INFO, ("Temp: %f, Humidity: %f", temp_reading, humidity_reading));
	warmup_mic();
}

static void warmup_dht()
{
	LOG(LL_INFO, ("Warmup dht"));
	mgos_gpio_write(mgos_sys_config_get_pins_transistor_dht(), 1);
	mgos_set_timer(10 * 1000 /* ms */, false, read_dht, NULL);
}

static void start_sensor_reading_sequence(void *arg)
{
	warmup_dht();
}

enum mgos_app_init_result mgos_app_init(void)
{
	mgos_gpio_setup_input(mgos_sys_config_get_pins_temp(), 0);
	mgos_gpio_setup_input(mgos_sys_config_get_pins_mic(), 0);
	mgos_gpio_setup_input(mgos_sys_config_get_pins_co2(), 0);

	mgos_gpio_setup_output(mgos_sys_config_get_pins_transistor_dht(), 0);
	mgos_gpio_setup_output(mgos_sys_config_get_pins_transistor_mic(), 0);
	mgos_gpio_setup_output(mgos_sys_config_get_pins_transistor_co2(), 0);

	mgos_adc_enable(mgos_sys_config_get_pins_mic());

	start_sensor_reading_sequence(NULL);
	mgos_set_timer(60 * 60 * 1000 /* ms */, MGOS_TIMER_REPEAT, start_sensor_reading_sequence, NULL);

	return MGOS_APP_INIT_SUCCESS;
}
