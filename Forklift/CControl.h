#pragma once
#include "pigpio.h"

#define STEPR 27
#define DIRR 17
#define ENR 22

#define STEPL 24
#define DIRL 25
#define ENL 23

// F denotes the front motors
#define PWMAF 18
#define INA2F 16
#define INA1F 12

#define PWMBF 13
#define INB2F 6
#define INB1F 5

#define STANDBYF 26

// B denotes the back motors
#define PWMAB 2
#define INA2B 3
#define INA1B 4

#define PWMBB 1
#define INB2B 0
#define INB1B 15

#define STANDBYB 14

/**
*
* @brief Allows for simple communication with the Tiva TM4C123 microcontroller
*
* This class is meant to contain all the variables related to
* the microcontroller I/O.
*
* @author Isaac Fediw
*
*/
class CControl {
public:
	/**
	* @brief possible types of communication
	*/
	enum type{DIGITAL = 0, ANALOG, SERVO, BUTTON, STEPPER, PWM};

	/** @brief CControl constructor
	*
	* @param comport none
	* @return nothing to return
	*/
	CControl();

	/** @brief CControl destructor
	*
	* @param comport The com port to communicate through
	* @return nothing to return
	*/
	~CControl();

	/** @brief Gets the specified analog input from the microcontroller
	*
	* @param channel The channel which the input is on
	* @param n Number of bit ADC (Ex. 12)
	* @param result The variable you want the result to be stored in
	* @return Returns the analog input as a % of the full scale
	*/
	float get_analog(int channel, int n, int& result);

	/** @brief Gets the specified input from the microcontroller
	*
	* @param type The type of input (DIGITAL, ANALOG, SERVO)
	* @param channel The channel which the input is on
	* @param result The variable you want the result to be stored in
	* @return Returns a bool. (True --> Data retrieved successfully) (False --> Data not retrieved)
	*/
	bool get_data(int type, int channel, int& result);

	/** @brief Sets the specified output from the microcontroller
	*
	* @param type The type of output (DIGITAL, ANALOG, SERVO, STEPPER)
	* @param channel The channel which the output is on. Is not used when type == STEPPER
	* @param val The value you want to send
	* @param dir The direction you want a stepper motor to rotate. Defaults to CW
	* @return Returns a bool. (True --> Data sent successfully) (False --> Data not sent)
	*/
	bool set_data(int type, int channel, int val, int dir = 1);

private:
    float servo_pos;
};
