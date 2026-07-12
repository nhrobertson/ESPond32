#ifndef CONFIG_H
#define CONFIG_H

#define NUM_OUTPUTS       4
#define NUM_INPUTS        1
#define NUM_DEVICES       (NUM_OUTPUTS + NUM_INPUTS)


#define NUM_PUMPS         2
#define NUM_VALVES        1
#define NUM_LIGHT         1
#define NUM_FLOAT         1

#define NUM_LEDS          (NUM_DEVICES + 1) //1 - Pump 1, 2 - Pump 2, 3 - Valve 1, 4 - Light 1, 5 - Float Sensor

#define DEBOUNCE_SAMPLES      5
#define MAX_FILL_MIN          25  //Max continuous fill duration before lockout (stuck float failsafe)
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

#define LIGHT1_SSR_GPIO       GPIO_NUM_11
#define LIGHT1_SW_A_GPIO      GPIO_NUM_12
#define LIGHT1_SW_B_GPIO      GPIO_NUM_13
#define LIGHT1_LED_PIX        3

#define FLOAT1_SENS_GPIO      GPIO_NUM_9
#define FLOAT1_LED_PIX        4


#define LED_DATA_GPIO         GPIO_NUM_10

#define RESET_BTN_GPIO        GPIO_NUM_14

#define SYS_LED_PIX           5


#define DEBUG_INDEPTH_LOG     1
#define DEBUG_GPIO_ENABLE     1

#define DEBUG_GPIO_OPERATE    GPIO_NUM_21   //task_operate      (Core 1, 50ms)
#define DEBUG_GPIO_EVAL       GPIO_NUM_1    //task_evaluate_cfg (Core 1, 250ms)
#define DEBUG_GPIO_IO         GPIO_NUM_2    //task_check_io     (Core 1, 10ms)
#define DEBUG_GPIO_LEAK       GPIO_NUM_38   //task_check_leak   (Core 1, 10ms)
#define DEBUG_GPIO_RESET      GPIO_NUM_47   //task_check_for_reset (Core 1, 50ms)
#define DEBUG_GPIO_NET        GPIO_NUM_48   //task_net_manager  (Core 0, event-driven)
#define DEBUG_GPIO_CFG        GPIO_NUM_35   //task_check_cfg    (Core 0, event-driven)
#define DEBUG_GPIO_TASKS      GPIO_NUM_36   //task_listen_for_task_event (Core 1, event-driven)
#define DEBUG_GPIO_SYS_LED    GPIO_NUM_37   //task_set_sys_light (Core 1, 1000ms)


#endif //CONFIG_H
