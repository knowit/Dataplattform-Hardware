#include <stdint.h>
#include <stdbool.h>
#include "mgos.h"

#define TIMEOUT 2000
#define MILLIS() (int64_t)(mgos_uptime_micros() / 1000)

long co2(uint8_t pin)
{

    uint64_t t0 = MILLIS();
    // wait while pulse is low
    while (mgos_gpio_read(pin) == 0)
    {
        if ((MILLIS() - t0) > TIMEOUT)
        {
            return 0;
        }
    }
    uint64_t t1 = MILLIS();
    // wait while pulse is high
    while (mgos_gpio_read(pin) == 1)
    {
        if ((MILLIS() - t1) > TIMEOUT)
        {
            return 0;
        }
    }

    uint64_t t2 = MILLIS();
    // wait while pulse is low
    while (mgos_gpio_read(pin) == 0)
    {
        if ((MILLIS() - t2) > TIMEOUT)
        {
            return 0;
        }
    }

    uint64_t t3 = MILLIS();

    // wait for pulse to reset to low
    while (mgos_gpio_read(pin) == 1)
    {
        if ((MILLIS() - t3) > TIMEOUT)
        {
            return 0;
        }
    }

    long th = t2 - t1;
    long tl = t3 - t2;
    LOG(LL_INFO, ("th:  %li, tl: %li", th, tl));
    long ppm = 5000L * (th - 2) / (th + tl - 4);
    return ppm;
}
