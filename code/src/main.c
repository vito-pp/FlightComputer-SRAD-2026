#include <stdio.h>

#include "pico/stdlib.h"
#include "board/myboard.h"
#include "adxl375.h"

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    printf("ADXL375 test start\n");

    if (!adxl375_init()) {
        printf("ADXL375 init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("ADXL375 init OK\n");

    while (true) {
	adxl375_sample_t sample;

	adxl375_poll_result_t result = adxl375_poll_sample(&sample);

	if (result == ADXL375_POLL_OK) {
		printf("ADXL x=%.2f y=%.2f z=%.2f g\n",
           	sample.x_g,
	        sample.y_g,
	        sample.z_g);
	} else if (result == ADXL375_POLL_NO_DATA) {
    	// Nothing new. Continue doing other work.
	} else {
    		printf("ADXL read error\n");
	}
	
        sleep_ms(100);
    }
}
