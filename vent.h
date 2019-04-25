#ifndef VENT_H
#define	VENT_H

// Contains miscellaneous vent board-specific code

#define MAX_LOOP_TIME_DIFF_ms 250

#define RED_LED_ON() (LATC5 = 0)
#define RED_LED_OFF() (LATC5 = 1)
#define WHITE_LED_ON() (LATC6 = 0)
#define WHITE_LED_OFF() (LATC6 = 1)
#define BLUE_LED_ON() (LATC7 = 0)
#define BLUE_LED_OFF() (LATC7 = 1)

void LED_init(void);

#endif	/* VENT_H */

