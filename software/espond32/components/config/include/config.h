#ifndef CONFIG_H
#define CONFIG_H

#define NUM_OUTPUTS       3
#define NUM_INPUTS        1
#define NUM_DEVICES       (NUM_OUTPUTS + NUM_INPUTS)


#define NUM_PUMPS         2
#define NUM_VALVES        1

#define NUM_FLOAT         1

#define NUM_LEDS          4 //1 - Pump 1, 2 - Pump 2, 3 - Valve 1, 4 - Float Sensor

//GPIO Definitions
#define PUMP1_SSR_GPIO        GPIO_NUM_4
#define PUMP1_SW_A_GPIO       GPIO_NUM_5
#define PUMP1_SW_B_GPIO       GPIO_NUM_6
#define PUMP1_LED_PIX         0

#define PUMP2_SSR_GPIO        GPIO_NUM_7
#define PUMP2_SW_A_GPIO       GPIO_NUM_15
#define PUMP2_SW_B_GPIO       GPIO_NUM_16
#define PUMP2_LED_PIX         1

#define VALVE1_SSR_GPIO       GPIO_NUM_17
#define VALVE1_SW_A_GPIO      GPIO_NUM_18
#define VALVE1_SW_B_GPIO      GPIO_NUM_8
#define VALVE1_LED_PIX        2

#define FLOAT1_SENS_GPIO      GPIO_NUM_9
#define FLOAT1_LED_PIX        3

#define LED_DATA_GPIO         GPIO_NUM_10

void func(void);

#endif //CONFIG_H
