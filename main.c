#include <stdio.h>
#include <stdlib.h>

#include "canlib/can.h"
#include "canlib/can_common.h"
#include "canlib/pic18f26k83/pic18f26k83_can.h"
#include "canlib/message_types.h"
#include "canlib/util/timing_util.h"

// MCC Generated I2C Driver
#include "mcc_generated_files/i2c1.h"
#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/adcc.h"
#include "mcc_generated_files/pin_manager.h"

#include "timer.h"

#include <xc.h>

#define RED_LED_ON() (LATC5 = 0)
#define RED_LED_OFF() (LATC5 = 1)
#define WHITE_LED_ON() (LATC6 = 0)
#define WHITE_LED_OFF() (LATC6 = 1)
#define BLUE_LED_ON() (LATC7 = 0)
#define BLUE_LED_OFF() (LATC7 = 1)

static void can_msg_handler(can_msg_t *msg);
static void send_status_ok(void);
#include "lin_actuator.h"


// Follows VALVE_STATE in message_types.h
// SHOULD ONLY BE MODIFIED IN ISR
static uint8_t requested_valve_state = VALVE_OPEN;

// global variables for debuging
int battery_voltage = 0;
int LINAC_POT = 0;
int current_draw = 0;
lin_actuator_states vent_state = nominal;

static void LED_init() {
    TRISC5 = 0;     //set C5 as output
    LATC5 = 1;      // turn the led off
    
    TRISC6 = 0;
    LATC6 = 1;      // turn the led off
    
    TRISC7 = 0;
    LATC7 = 1;      // turn the led off
}



int check_battery_voltage(void){    //returns mV
    return ADCC_GetSingleConversion(channel_VBAT)*3.95; // scaled by value calculated via testing
}

int check_current_draw(void){       //returns mA
    return ADCC_GetSingleConversion(channel_VSENSE)/20; //i =v/r r = 0.2 ohms, v = VSENCE/100
}

int main(int argc, char** argv) {
    // MCC generated initializer
    SYSTEM_Initialize();
    OSCILLATOR_Initialize();
    
    FVR_Initialize();
    ADCC_Initialize();
    ADCC_DisableContinuousConversion();
    // I2C1 Pins: SCL1 -> RC3, SDA1 -> RC4
    I2C1_Initialize();  
    LED_init();
    timer0_init();
    
    // Enable global interrupts
    INTCON0bits.GIE = 1;
    
    // Set up CAN TX
    TRISC0 = 0;
    RC0PPS = 0x33;
    
    // Set up CAN RX
    TRISC1 = 1;
    ANSELC1 = 0;
    CANRXPPS = 0x11;
    
    // set up CAN module
    can_timing_t can_setup;
    can_generate_timing_params(_XTAL_FREQ, &can_setup);
    can_init(&can_setup, can_msg_handler);
    lin_actuator_init();
    
    set_DACs();
    
    close_vent();
    while (1) {
        BLUE_LED_OFF();
        __delay_ms(100);
        BLUE_LED_ON();
        __delay_ms(100);
        
        send_status_ok();

        // For debugging purposes, put the valve state on the white LED
        switch (requested_valve_state) {
            case VALVE_OPEN:
                WHITE_LED_ON();
                break;
            case VALVE_CLOSED:
                WHITE_LED_OFF();
                break;
            default:
                break;
        }
        battery_voltage = check_battery_voltage();// returns mV
        current_draw = check_current_draw();// returns mA
        LINAC_POT = ADCC_GetSingleConversion(channel_LINAC_POT);// returns mV
        
        vent_state = check_vent_status();
    }
    
    return (EXIT_SUCCESS);
}

static void interrupt interrupt_handler() {
    if (PIR5) {
        can_handle_interrupt();
    }

    // Timer0 has overflowed - update millis() function
    // This happens approximately every 500us
    if (PIE3bits.TMR0IE == 1 && PIR3bits.TMR0IF == 1) {
        timer0_handle_interrupt();
        PIR3bits.TMR0IF = 0;
    }
}

static void can_msg_handler(can_msg_t *msg) {
    uint16_t msg_type = get_message_type(msg);
    switch (msg_type) {
        case MSG_GENERAL_CMD:
            // nothing right now
            break;

        case MSG_VENT_VALVE_CMD:
            // see message_types.h for message format
            requested_valve_state = msg->data[3];
            break;

        case MSG_LEDS_ON:
            RED_LED_ON();
            BLUE_LED_ON();
            WHITE_LED_ON();
            break;

        case MSG_LEDS_OFF:
            RED_LED_OFF();
            BLUE_LED_OFF();
            WHITE_LED_OFF();
            break;

        // all the other ones - do nothing
        case MSG_INJ_VALVE_CMD:
        case MSG_DEBUG_MSG:
        case MSG_DEBUG_PRINTF:
        case MSG_VENT_VALVE_STATUS:
        case MSG_INJ_VALVE_STATUS:
        case MSG_SENSOR_ACC:
        case MSG_SENSOR_GYRO:
        case MSG_SENSOR_MAG:
        case MSG_SENSOR_ANALOG:
        case MSG_GENERAL_BOARD_STATUS:
            break;

        // illegal message type - should never get here
        default:
            // send a message or something
            break;
    }
}

// Send a CAN message with nominal status
static void send_status_ok(void) {
    can_msg_t board_stat_msg;
    board_stat_msg.sid = MSG_GENERAL_BOARD_STATUS | BOARD_UNIQUE_ID;

    // capture the most recent timestamp
    uint32_t last_millis = millis();

    // paste in the timestamp one byte at a time. Most significant byte goes in data[0].
    board_stat_msg.data[0] = (last_millis >> 16) & 0xff;
    board_stat_msg.data[1] = (last_millis >> 8) & 0xff;
    board_stat_msg.data[2] = (last_millis >> 0) & 0xff;

    // set the error code
    board_stat_msg.data[3] = E_NOMINAL;

    // 3 byte timestamp + 1 byte error code
    board_stat_msg.data_len = 4;

    // send it off at low priority
    can_send(&board_stat_msg, 0);
}

