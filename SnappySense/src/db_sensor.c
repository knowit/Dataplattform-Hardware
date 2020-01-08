#include "mgos.h"
#include "mgos_adc.h"
#include <math.h>

#define SAMPLE_RATE 50
#define DB_REFERENCE 21
#define ANALOG_REFERENCE 30

#define UPTIME() (double) (mgos_uptime_micros() / 1000)
#define DB(max, min) (double) (20 * log10((double)(max - min) / ANALOG_REFERENCE) + DB_REFERENCE)


double get_noise(uint8_t sensor_pin, int milliseconds){

	double startTime = UPTIME();

	double db = 0;
	int numberOfSamples = 0;

	do {
		double startSampleTime = UPTIME();
		int min = 4095;
		int max = 0;
	
		do {
			float raw = mgos_adc_read(sensor_pin);
			if(raw < min){
				min = raw;
			}
			if(raw > max){
				max = raw;
			}

		} while(UPTIME() < startSampleTime + SAMPLE_RATE);

		db = db + DB(max, min);
		numberOfSamples++;
		mgos_wdt_feed();

	} while(UPTIME() < startTime + milliseconds);

	return (double) (db / numberOfSamples);
}
