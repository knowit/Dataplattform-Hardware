#include <stdint.h>
#include <stdbool.h>
#include "mgos.h"

#define HIGH 5000
#define TIMEOUT 1004000

#define PPM(th, tl) HIGH * (th - 2) / (th + tl - 4)

#define UPTIME() (uint64_t) (1000000 * mgos_uptime())


static uint32_t pulseInLong(uint8_t pin, uint8_t state, uint32_t timeout)
{

    uint64_t startMicros = UPTIME();


    // wait for any previous pulse to end
    while (state == mgos_gpio_read(pin)) {
        if ((UPTIME() - startMicros) > timeout) {
            return 0;
        }
    }

    // wait for the pulse to start
    while (state != mgos_gpio_read(pin)) {
        if ((UPTIME() - startMicros) > timeout) {
            return 0;
        }
    }

    uint64_t start = UPTIME();

    // wait for the pulse to stop
    while (state == mgos_gpio_read(pin)) {
        if ((UPTIME() - startMicros) > timeout) {
            return 0;
        }
    }

    return (uint32_t) (UPTIME() - start);
}

long int co2(uint8_t pwm_pin)
{
	unsigned long th, tl, ppm_pwm = 0;

	LOG(LL_INFO, ("Getting co2"));

	do {
		th = pulseInLong(pwm_pin, true, TIMEOUT) / 1000;
		mgos_wdt_feed();
		tl = 1004 - th;

		ppm_pwm = PPM(th, tl);

	} while(th == 0);


	return ppm_pwm;
}
