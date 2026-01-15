#include <iostream>
#include <SDL.h>
#include <SDL_gamecontroller.h>
#include <map>
#include <array>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <chrono>
#include <thread>

#include <opencv2/opencv.hpp>

#include "CControl.h"

using namespace cv;
using namespace std;

enum DIRECTION {RIGHT = 0, FORWARD, LEFT, BACKWARD};
enum WHEEL {FRONT_LEFT = 0, FRONT_RIGHT, BACK_LEFT, BACK_RIGHT};

// {button, {channel, steps, dir}}
// channel == 0 --> right fork
// channel == 1 --> left fork
map<Uint8, array<int, 3>> buttons;

void turn_wheel(int channel, int duty_cycle, int dir);
void all_wheels_off();
void handle_laterals(SDL_Event e);

int move_forklift(Uint8 button, int &facing, int duty_cycle = 200);
void record(int facing);
void play_back();

CControl control;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        cout << "Initialization Error: " << SDL_GetError() << "\n";\
        SDL_Quit();
        return 1;
    }
    SDL_GameController *controller = nullptr;

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            break;
        }
    }

    int level_steps = 200;
    int fine_steps = 10;

    buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = {0, level_steps, 1};
    buttons[SDL_CONTROLLER_BUTTON_RIGHTSTICK] =    {0, level_steps, 0};
    buttons[SDL_CONTROLLER_BUTTON_LEFTSTICK] =     {1, level_steps, 1};
    buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] =  {1, level_steps, 0};
    buttons[SDL_CONTROLLER_BUTTON_DPAD_UP] =       {1, fine_steps,  1};
    buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT] =     {1, fine_steps,  0};
    buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] =    {0, fine_steps,  0};
    buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN] =     {0, fine_steps,  1};

    SDL_Event e;
    bool quit = false;

    int facing = FORWARD; // facing forward by default

    all_wheels_off();

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                if (buttons.find(e.cbutton.button) != buttons.end()) {
                    control.set_data(control.STEPPER, buttons[e.cbutton.button][0], buttons[e.cbutton.button][1] == 200 ? 1300 : 200, buttons[e.cbutton.button][2]);
                }

                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    record(facing);
                } else if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) {
                    play_back();
                } else {
                    int duration = move_forklift(e.cbutton.button, facing);

                    if (duration > 0) {
                        SDL_Delay(duration);
                        all_wheels_off();
                    }
                }
            } else if (e.type == SDL_JOYAXISMOTION) {
                handle_laterals(e);
            }

            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }
    }

    all_wheels_off();
    SDL_GameControllerClose(controller);
    SDL_Quit();

    return 0;
}

void all_wheels_off() {
    control.set_data(control.DIGITAL, STANDBYF, 0);
    control.set_data(control.DIGITAL, STANDBYB, 0);
}

void turn_wheel(int channel, int duty_cycle, int dir) {
    int standby, in1, in2, in1dir, in2dir;

    // PWM, channel (0 -> A, 1 -> B), duty cycle, frequency
    control.set_data(control.PWM, channel, abs(duty_cycle), 100);

    if (channel == FRONT_LEFT) {
        standby = STANDBYF;
        in1 = INA1F;
        in2 = INA2F;
    } else if (channel == FRONT_RIGHT) {
        standby = STANDBYF;
        in1 = INB1F;
        in2 = INB2F;
    } else if (channel == BACK_LEFT) {
        standby = STANDBYB;
        in1 = INA1B;
        in2 = INA2B;
    } else if (channel == BACK_RIGHT) {
        standby = STANDBYB;
        in1 = INB1B;
        in2 = INB2B;
    } else return;

    if (dir == FORWARD) {
        in1dir = 1;
        in2dir = 0;
    } else if (dir == BACKWARD) {
        in1dir = 0;
        in2dir = 1;
    } else return;

    control.set_data(control.DIGITAL, standby, 1);
    control.set_data(control.DIGITAL, in1, in1dir);
    control.set_data(control.DIGITAL, in2, in2dir);
}

int move_forklift(Uint8 button, int &facing, int duty_cycle) {
    int turn_90_duration = 950;
    int turn_180_duration = 1875;
    int duration = 0;

    // buttons to control turning
    if (button == SDL_CONTROLLER_BUTTON_B) {
        switch (facing) {
            case RIGHT:
                // don't turn
                duration = 0;
                break;
            case FORWARD:
                // right 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_90_duration;
                break;
            case LEFT:
                // left 180deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_180_duration;
                break;
            case BACKWARD:
                // left 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_90_duration;
                break;
            default: break;
        }
        facing = RIGHT;
    } else if (button == SDL_CONTROLLER_BUTTON_Y) {
        switch (facing) {
            case RIGHT:
                // left 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_90_duration;
                break;
            case FORWARD:
                // don't turn
                duration = 0;
                break;
            case LEFT:
                // right 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_90_duration;
                break;
            case BACKWARD:
                // left 180deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_180_duration;
            default: break;
        }
        facing = FORWARD;
    }  else if (button == SDL_CONTROLLER_BUTTON_X) {
        switch (facing) {
            case RIGHT:
                // right 180deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_180_duration;
                break;
            case FORWARD:
                // left 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_90_duration;
                break;
            case LEFT:
                // don't turn
                duration = 0;
                break;
            case BACKWARD:
                // right 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_90_duration;
            default: break;
        }
        facing = LEFT;
    }  else if (button == SDL_CONTROLLER_BUTTON_A) {
        switch (facing) {
            case RIGHT:
                // right 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_90_duration;
                break;
            case FORWARD:
                // right 180deg
                turn_wheel(FRONT_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, BACKWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, FORWARD);
                turn_wheel(BACK_LEFT, duty_cycle, FORWARD);

                duration = turn_180_duration;
                break;
            case LEFT:
                // left 90deg
                turn_wheel(FRONT_RIGHT, duty_cycle, FORWARD);
                turn_wheel(BACK_RIGHT, duty_cycle, FORWARD);
                turn_wheel(FRONT_LEFT, duty_cycle, BACKWARD);
                turn_wheel(BACK_LEFT, duty_cycle, BACKWARD);

                duration = turn_90_duration;
                break;
            case BACKWARD:
                // don't turn
                duration = 0;
                break;
            default: break;
        }
        facing = BACKWARD;
    }

    return duration;
}

int dc = 0;
void handle_laterals(SDL_Event e) {
    if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
        // map the value to -200->200 for the joystick
        dc = (int)((400.0 / 65535.0) * (e.caxis.value + 32768) - 200);
        if (dc > 0 && dc < 140) dc = 0;
        if (dc < 0 && dc > -140) dc = 0;

        if (dc >= 0) {
            turn_wheel(FRONT_RIGHT, dc, BACKWARD);
            turn_wheel(FRONT_LEFT, dc, FORWARD);
            turn_wheel(BACK_RIGHT, dc, FORWARD);
            turn_wheel(BACK_LEFT, dc != 0 ? abs(dc) - 40 : dc, BACKWARD);
        } else {
            turn_wheel(FRONT_RIGHT, dc, FORWARD);
            turn_wheel(FRONT_LEFT, dc, BACKWARD);
            turn_wheel(BACK_RIGHT, dc, BACKWARD);
            turn_wheel(BACK_LEFT, dc != 0 ? abs(dc) - 40 : dc, FORWARD);
        }
    } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
        double start = getTickCount();
        // map the value to 0->200 for the right trigger
        dc = (int)((200.0 / 65535.0) * (e.caxis.value + 32768));
        if (dc > 0 && dc < 50) dc = 0;
        if (dc < 0) dc = 0;

        turn_wheel(FRONT_RIGHT, dc, FORWARD);
        turn_wheel(FRONT_LEFT, dc, FORWARD);
        turn_wheel(BACK_RIGHT, dc, FORWARD);
        turn_wheel(BACK_LEFT, dc, FORWARD);
    } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
        // map the value to 0->200 for the left trigger
        dc = (int)((200.0 / 65535.0) * (e.caxis.value + 32768));
        if (dc > 0 && dc < 50) dc = 0;
        if (dc < 0) dc = 0;

        turn_wheel(FRONT_RIGHT, dc, BACKWARD);
        turn_wheel(FRONT_LEFT, dc, BACKWARD);
        turn_wheel(BACK_RIGHT, dc, BACKWARD);
        turn_wheel(BACK_LEFT, dc, BACKWARD);
    }
}

void record(int facing) {
    bool finish_recording = false;
    ofstream outfile;
    SDL_Event e;

    outfile.open("Recording.txt");
    cout << "Recording Started\n";

    double start = getTickCount();
    int last_dir = -1;

    while (!finish_recording) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                if (buttons.find(e.cbutton.button) != buttons.end()) {
                    if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " " + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                    start = getTickCount();
                    last_dir = -1;

                    outfile << "STEPPER " << buttons[e.cbutton.button][0] << " " << buttons[e.cbutton.button][1] << " " << buttons[e.cbutton.button][2] << " -1 -1\n";
                    control.set_data(control.STEPPER, buttons[e.cbutton.button][0], buttons[e.cbutton.button][1], buttons[e.cbutton.button][2]);
                }

                int prev_facing = facing;

                int duration = move_forklift(e.cbutton.button, facing);

                if (duration > 0) {
                    SDL_Delay(duration);
                    all_wheels_off();

                    if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " " + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                    start = getTickCount();
                    last_dir = -1;

                    outfile << "DC -1 200 " << facing << " " << duration << " " << prev_facing << "\n";
                }

                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) finish_recording = true;
            } else if (e.type == SDL_JOYAXISMOTION) {
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                    if (e.caxis.value > 16384) {
                        turn_wheel(FRONT_RIGHT, 200, BACKWARD);
                        turn_wheel(FRONT_LEFT, 200, FORWARD);
                        turn_wheel(BACK_RIGHT, 200, FORWARD);
                        turn_wheel(BACK_LEFT, 160, BACKWARD);

                        if (last_dir != RIGHT) {
                            if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " " + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                            start = getTickCount();
                            last_dir = RIGHT;
                        }
                    } else if (e.caxis.value < -16384) {
                        turn_wheel(FRONT_RIGHT, 200, FORWARD);
                        turn_wheel(FRONT_LEFT, 200, BACKWARD);
                        turn_wheel(BACK_RIGHT, 200, BACKWARD);
                        turn_wheel(BACK_LEFT, 160, FORWARD);

                        if (last_dir != LEFT) {
                            if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                            start = getTickCount();
                            last_dir = LEFT;
                        }
                    } else {
                        if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                        start = getTickCount();
                        last_dir = -1;
                        all_wheels_off();
                    }
                } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                    if (e.caxis.value > 0) {
                        turn_wheel(FRONT_RIGHT, 200, FORWARD);
                        turn_wheel(FRONT_LEFT, 200, FORWARD);
                        turn_wheel(BACK_RIGHT, 200, FORWARD);
                        turn_wheel(BACK_LEFT, 200, FORWARD);

                        if (last_dir != FORWARD) {
                            if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                            start = getTickCount();
                            last_dir = FORWARD;
                        }
                    } else {
                        if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                        start = getTickCount();
                        last_dir = -1;
                        all_wheels_off();
                    }
                } else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
                    if (e.caxis.value > 0) {
                        turn_wheel(FRONT_RIGHT, 200, BACKWARD);
                        turn_wheel(FRONT_LEFT, 200, BACKWARD);
                        turn_wheel(BACK_RIGHT, 200, BACKWARD);
                        turn_wheel(BACK_LEFT, 200, BACKWARD);

                        if (last_dir != BACKWARD) {
                            if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                            start = getTickCount();
                            last_dir = BACKWARD;
                        }
                    } else {
                        if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1\n";
                        start = getTickCount();
                        last_dir = -1;
                        all_wheels_off();
                    }
                }
            }
        }
    }

    if (last_dir != -1) outfile << "DC -1 200 " + to_string(last_dir) + " "  + to_string((getTickCount() - start) / getTickFrequency()) + " -1";

    // turn front right wheel slightly to signify end of recording
    turn_wheel(FRONT_RIGHT, 200, FORWARD);
    SDL_Delay(100);
    all_wheels_off();

    cout << "Recording Finished\n";
    outfile.close();
}

// motor_type, channel, steps (duty cycle), dir, duration, facing
void play_back() {
    ifstream infile;
    bool finished_playing = false;

    string motor_type;
    int channel, steps /*also duty_cycle*/, dir, facing, delay;
    double duration;

    infile.open("Recording.txt");


    while (!finished_playing) {
        infile >> motor_type >> channel >> steps >> dir >> duration >> facing;

        if (motor_type == "STEPPER") {
            if (!infile.eof()) control.set_data(control.STEPPER, channel, steps, dir);
        } else if (motor_type == "DC") {
            if (facing != -1) {
                if (dir == FORWARD) delay = move_forklift(SDL_CONTROLLER_BUTTON_Y, facing);
                else if (dir == RIGHT) delay = move_forklift(SDL_CONTROLLER_BUTTON_B, facing);
                else if (dir == LEFT) delay = move_forklift(SDL_CONTROLLER_BUTTON_X, facing);
                else if (dir == BACKWARD) delay = move_forklift(SDL_CONTROLLER_BUTTON_A, facing);

                SDL_Delay(delay);
                all_wheels_off();
            } else {
                if (dir == FORWARD) {
                    turn_wheel(FRONT_RIGHT, steps, FORWARD);
                    turn_wheel(FRONT_LEFT, steps, FORWARD);
                    turn_wheel(BACK_RIGHT, steps, FORWARD);
                    turn_wheel(BACK_LEFT, steps, FORWARD);
                } else if (dir == BACKWARD) {
                    turn_wheel(FRONT_RIGHT, steps, BACKWARD);
                    turn_wheel(FRONT_LEFT, steps, BACKWARD);
                    turn_wheel(BACK_RIGHT, steps, BACKWARD);
                    turn_wheel(BACK_LEFT, steps, BACKWARD);
                } else if (dir == RIGHT) {
                    turn_wheel(FRONT_RIGHT, steps, BACKWARD);
                    turn_wheel(FRONT_LEFT, steps, FORWARD);
                    turn_wheel(BACK_RIGHT, steps, FORWARD);
                    turn_wheel(BACK_LEFT, steps != 0 ? abs(steps) - 40 : steps, BACKWARD);
                } else if (dir == LEFT) {
                    turn_wheel(FRONT_RIGHT, steps, FORWARD);
                    turn_wheel(FRONT_LEFT, steps, BACKWARD);
                    turn_wheel(BACK_RIGHT, steps, BACKWARD);
                    turn_wheel(BACK_LEFT, steps != 0 ? abs(steps) - 40 : steps, FORWARD);
                }

                if (!infile.eof()) {
                    usleep(duration * 1000000);
                    all_wheels_off();
                    usleep(200000);
                }
            }
        }

        finished_playing = infile.eof();
    }

    infile.close();
    cout << "Play Back Finished\n";
    all_wheels_off();
}
