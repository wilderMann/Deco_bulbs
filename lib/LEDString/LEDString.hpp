/*
   This class is far from perfect.
   It uses global ISR for the ticker.

   It implements an pseudo PWM through the ticker function.
   Solution is only in ms and to prevent flickering it only dimms the light
   in 10 steps.

   HOW TO USE:
   LEDString lamps(LAMPS_PIN);
   lamps.init();

   Feel free to change the standart DIMMSPEED.

 */

#ifndef LEDSTRING_HPP_
#define LEDSTRING_HPP_

#include <Ticker.h>
#include <Arduino.h>

#define DIMMSPEED 1000
#define LOWERLIMIT 1

const uint16_t pwmtable_10[100] =
{
    0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
    6, 6, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21,
    23, 24, 26, 28, 30, 32, 34, 37, 39, 42, 45, 48, 52, 56, 60, 64, 69, 73,
    79, 84, 90, 97, 104, 111, 119, 128, 137, 147, 157, 169, 181, 194, 208,
    223, 239, 256, 274, 294, 315, 338, 362, 388, 416, 445, 477, 512, 548,
    588, 630, 675, 723, 775, 831, 891, 955, 1023
};

class LEDString {

int duty_save = 0;
int duty_goal = 0;
int duty_old = 0;
int direction = -1;
unsigned long steps = 0;
uint8_t status = 2;


public:
Ticker tick;
uint8_t Pin = 0;
uint8_t state = 0;
LEDString(uint8_t LEDPin);
void init();
void set(int duty);
void move(int duty, int time_ms = DIMMSPEED);
void on();
void off();
bool getStatus();
int getDuty();
void up();
void down();
bool ismoving();
int getDirection();
};

namespace LEDSTRING
{
void tickHandlerLEDStr(LEDString *obj);

}

#endif
