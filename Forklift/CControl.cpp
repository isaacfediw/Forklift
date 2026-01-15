#include "CControl.h"
#include "pigpio.h"
#include <sstream>
#include <iostream>
#include <math.h>
#include <unistd.h>

#define CE0 8

using namespace std;

CControl::CControl() {servo_pos = 0;}

CControl::~CControl() {gpioTerminate();}

float CControl::get_analog(int channel, int n, int& result) {
    get_data(ANALOG, channel, result);

    return (result / pow(2, n)) * 100;
}

bool CControl::get_data(int type, int channel, int& result) {
    if (gpioInitialise() < 0) {
        gpioTerminate();
        return false;
    }


    if (type == ANALOG) {
        unsigned char inBuf[3];
        char cmd[] = {1, channel == 0 ? 0b10000000 : 0b10010000, 0}; // 0b1XXX0000 where XXX is the channel

        int handle = spiOpen(0, 200000, 3); // Mode 0, 200kHz

        spiXfer(handle, cmd, (char*) inBuf, 3); // Transfer 3 bytes
        result = ((inBuf[1] & 3) << 8) | inBuf[2]; // Format 10 bits

        spiClose(handle);
    }

    if (type == DIGITAL) {
        gpioSetMode(channel, PI_INPUT);
        result = gpioRead(channel);
    }

    if (type == SERVO) {
        result = servo_pos;
    }

    return true;
}


bool CControl::set_data(int type, int channel, int val, int dir) {
    if (gpioInitialise() < 0) return false;
    if (channel != -1) gpioSetMode(channel, PI_OUTPUT);

    if (type == DIGITAL) {
        gpioWrite(channel, val);
    }

    if (type == SERVO) {
        gpioServo(channel, (2.0/180 * val + 0.5) * 1000);
        servo_pos = val;
    }

    if (type == STEPPER) {
        if (channel == 0) { // right fork
         gpioSetMode(STEPR, PI_OUTPUT);
         gpioSetMode(DIRR, PI_OUTPUT);
         gpioSetMode(ENR, PI_OUTPUT);

         gpioWrite(DIRR, dir);
         gpioWrite(ENR, 0);

         for (int i = 0; i < val; i++) { //val is # of steps
            gpioWrite(STEPR, 1);
            usleep(500);
            gpioWrite(STEPR, 0);
            usleep(500);
         }

         gpioWrite(ENR, 1);
        }

        if (channel == 1) { // left fork
         gpioSetMode(STEPL, PI_OUTPUT);
         gpioSetMode(DIRL, PI_OUTPUT);
         gpioSetMode(ENL, PI_OUTPUT);

         gpioWrite(DIRL, dir);
         gpioWrite(ENL, 0);

         for (int i = 0; i < val; i++) { //val is # of steps
            gpioWrite(STEPL, 1);
            usleep(500);
            gpioWrite(STEPL, 0);
            usleep(500);
         }

         gpioWrite(ENL, 1);
        }
    }

    // channel --> 0 for A, 1 for B
    // val --> duty cycle
    // dir --> frequency
    if (type == PWM) {
        int pin = PWMAF;
        if (channel == 1) pin = PWMBF;
        else if (channel == 2) pin = PWMAB;
        else if (channel == 3) pin = PWMBB;

        gpioSetMode(pin, PI_OUTPUT);
        gpioSetPWMfrequency(pin, dir);
        gpioPWM(pin, val);
    }

    return true;
}
