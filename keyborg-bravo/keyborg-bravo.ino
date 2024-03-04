// Arduino sketch to accessorize a keyboard.
// Copyright (C) 2022 Olivier Dagenais. All rights reserved.
// Licensed under the GNU General Public License. See LICENSE in the project
// root.

#include "Keyboard.h"
#include "Mouse.h"

typedef void (*callback_no_params)(void);


// imported and adapted from https://github.com/LSChyi/blackberry-mini-trackball/blob/f5187f980e1fb109c579be2ead443965f84aaf8e/blackberry_mini_trackball/blackberry_mini_trackball.ino
class Direction {
public:
  Direction(int pin1, int pin2) {
    this->pins[0] = pin1;
    this->pins[1] = pin2;
    pinMode(this->pins[0], INPUT);
    pinMode(this->pins[1], INPUT);
  };
  int read_action() {
    for(int i = 0; i < 2; ++i) {
      this->current_actions[i] = digitalRead(this->pins[i]);
      this->current_action_times[i] = millis();
      if(this->current_actions[i] != this->last_actions[i]) {
        this->last_actions[i] = this->current_actions[i];
        exponential = (exponential_bound - (this->current_action_times[i] - this->last_action_times[i]));
        exponential = (exponential > 0) ? exponential : 1;
        move_multiply = exponential_base;
        for(int i = 0; i < exponential; ++i) {
          move_multiply *= exponential_base;
        }
        this->last_action_times[i] = this->current_action_times[i];
        if(i == 0) {
          return (-1) * base_move_pixels * move_multiply;
        } else {
          return base_move_pixels * move_multiply;
        }
      }
    }
    return 0;
  };
private:
  int pins[2];
  int current_actions[2];
  int last_actions[2];
  int  exponential;
  double move_multiply;
  unsigned long current_action_times[2];
  unsigned long last_action_times[2];
  // these used to be declared via #define
  int base_move_pixels = 5;
  int exponential_bound = 15;
  double exponential_base = 1.2;
};

class Button {
  protected:
    const uint8_t _buttonPin;
    const callback_no_params _pressed;
    const callback_no_params _released;

    volatile bool _currentButtonState;

    virtual bool isPressed() {
        int v = digitalRead(_buttonPin);
        return (v == 0);
    }

  public:
    Button(uint8_t buttonPin,
           callback_no_params pressed,
           callback_no_params released)
        : _buttonPin(buttonPin), _pressed(pressed), _released(released) {
        _currentButtonState = false;
        pinMode(_buttonPin, INPUT_PULLUP);
    }

    void scan() {
        bool newState = isPressed();
        if (_currentButtonState) { // button was previously pressed
            if (!newState) {       // now it's released
                _currentButtonState = false;
                if (_released != NULL) {
                    _released();
                }
            }
        }
        else {              // button wasn't pressed
            if (newState) { // now it's pressed
                _currentButtonState = true;
                if (_pressed != NULL) {
                    _pressed();
                }
            }
        }
    }
};

class Encoder {
  protected:
    const uint8_t _clockPin;
    const uint8_t _dataPin;
    const callback_no_params _clockwise;
    const callback_no_params _counter;

    volatile bool _currentClockState;

  public:
    Encoder(uint8_t clockPin,
            uint8_t dataPin,
            callback_no_params clockwise,
            callback_no_params counter)
        : _clockPin(clockPin),
          _dataPin(dataPin),
          _clockwise(clockwise),
          _counter(counter) {
        _currentClockState = false;
        pinMode(_clockPin, INPUT_PULLUP);
        pinMode(_dataPin, INPUT_PULLUP);
    }

    void scan() {
        bool newClock = digitalRead(_clockPin) == 0;
        if (_currentClockState) { // clock was previously active
            if (!newClock) {      // now it's inactive
                _currentClockState = false;
            }
        }
        else {              // clock wasn't active
            if (newClock) { // now it's active
                _currentClockState = true;
                bool newData = digitalRead(_dataPin) == 0;
                if (newData) {
                    if (_clockwise != NULL) {
                        _clockwise();
                    }
                }
                else {
                    if (_counter != NULL) {
                        _counter();
                    }
                }
            }
        }
    }
};

class AnalogAxis {
  protected:
    const uint8_t _analogPin;
    const uint16_t _minAnalogValue;
    const uint16_t _maxAnalogValue;
    const uint16_t _range;
    const uint16_t _threshold;
    const uint16_t _center;
    const uint8_t _throttle;
    const float _acceleration;
    float _multiplier = 1;

  public:
    AnalogAxis(uint8_t analogPin,
               int16_t minAnalogValue,
               int16_t maxAnalogValue,
               uint16_t range,
               uint16_t threshold,
               uint8_t throttle,
               float acceleration)
        : _analogPin(analogPin),
          _minAnalogValue(minAnalogValue),
          _maxAnalogValue(maxAnalogValue),
          _range(range),
          _threshold(threshold),
          _center(range / 2),
          _throttle(throttle),
          _acceleration(acceleration) {
        // Do I need to initialize the pin?
    }

    int16_t read(uint8_t percent) {
        if (0 != (percent % _throttle)) {
            return 0;
        }
        int16_t reading = analogRead(_analogPin);
        int16_t adjusted =
            map(reading, _minAnalogValue, _maxAnalogValue, 0, _range);
        int16_t distance = adjusted - _center;
        if (abs(distance) < _threshold) {
            distance = 0;
            _multiplier = 1;
        }
        distance = distance * _multiplier;
        _multiplier += _acceleration;
        return distance;
    }
};

class AnalogLight {
  protected:
    const uint8_t _analogPin;
    const uint8_t _maxBrightness;
    unsigned long _onUntilMillis = 0;
    unsigned long _offUntilMillis = 0;
    unsigned long _blinkCycles = 0;
    unsigned long _blinkDelayMillis = 0;

  public:
    AnalogLight(uint8_t analogPin, uint8_t maxBrightness)
        : _analogPin(analogPin), _maxBrightness(maxBrightness) {
        pinMode(_analogPin, OUTPUT);
    }

    void turnOff(unsigned long offUntilMillis = 0) {
        _offUntilMillis = offUntilMillis;
        _onUntilMillis = 0;
        set(0);
    }

    void turnOn(unsigned long onUntilMillis = 0) {
        _onUntilMillis = onUntilMillis;
        _offUntilMillis = 0;
        set(_maxBrightness);
    }

    void blink(unsigned long blinkCycles = 1,
               unsigned long blinkDelayMillis = 200) {
        unsigned long nowMillis = millis();
        _blinkCycles = (blinkCycles - 1) * 2;
        _blinkDelayMillis = blinkDelayMillis;
        unsigned long onUntilMillis = nowMillis + _blinkDelayMillis;
        turnOn(onUntilMillis);
    }

    void tick() {
        unsigned long nowMillis = millis();
        if (_onUntilMillis != 0) {
            if (nowMillis >= _onUntilMillis) {
                _onUntilMillis = 0;
                unsigned long offUntilMillis = 0;
                if (_blinkCycles > 0) {
                    offUntilMillis = nowMillis + _blinkDelayMillis;
                    _blinkCycles--;
                }
                turnOff(offUntilMillis);
            }
        }
        if (_offUntilMillis != 0) {
            if (nowMillis >= _offUntilMillis) {
                _offUntilMillis = 0;
                unsigned long onUntilMillis = 0;
                if (_blinkCycles > 0) {
                    onUntilMillis = nowMillis + _blinkDelayMillis;
                    _blinkCycles--;
                }
                turnOn(onUntilMillis);
            }
        }
    }

    void set(uint8_t brightness) { analogWrite(_analogPin, brightness); }
};

const int NUM_BUTTONS = 3;
Button *_buttons[NUM_BUTTONS];

Encoder *_wheel;

const uint8_t MAX_BRIGHTNESS = 8;
AnalogLight *_redLight = new AnalogLight(3, MAX_BRIGHTNESS);
AnalogLight *_greenLight = new AnalogLight(6, MAX_BRIGHTNESS);
AnalogLight *_blueLight = new AnalogLight(9, MAX_BRIGHTNESS);
AnalogLight *_ballLight = new AnalogLight(5, 255);
const int NUM_LIGHTS = 4;
AnalogLight *_lights[NUM_LIGHTS];

const int NUM_ANALOG_AXIS = 4;
AnalogAxis *_analogAxis[NUM_ANALOG_AXIS];

bool _mouseActive = false;
void toggleMouseActive() { _mouseActive = !_mouseActive; }

void leftPress() { Mouse.press(MOUSE_LEFT); }
void leftRelease() { Mouse.release(MOUSE_LEFT); }

void middlePress() { Mouse.press(MOUSE_MIDDLE); }
void middleRelease() { Mouse.release(MOUSE_MIDDLE); }

void rightPress() { Mouse.press(MOUSE_RIGHT); }
void rightRelease() { Mouse.release(MOUSE_RIGHT); }

void clockwise() { Mouse.move(0, 0, 1); }
void counter() { Mouse.move(0, 0, -1); }

bool _muteState = false;
void toggleMuteMomentary() {
    _muteState = !_muteState;
    notifyMuteState();
    _greenLight->blink(2);
}

void notifyMuteState() {
    Keyboard.press(KEY_LEFT_ALT);
    Keyboard.press('k');
    Keyboard.releaseAll();
}

int x_move, y_move;
Direction x_direction(8, 7);
Direction y_direction(19, 20);

void setup() {
    _buttons[0] = new Button(2, &toggleMuteMomentary, NULL);
    _buttons[1] = new Button(14, &middlePress, &middleRelease);
    _buttons[2] = new Button(21, &rightPress, &rightRelease);
    _wheel = new Encoder(15, 16, &clockwise, &counter);

    _lights[0] = _redLight;
    _lights[1] = _greenLight;
    _lights[2] = _blueLight;
    _lights[3] = _ballLight;

    _redLight->blink(10);
    _blueLight->blink(1, 1000);
    _ballLight->turnOn();

    //Serial.begin(115200);
    Mouse.begin();
}

void loop() {
    for (uint8_t b = 0; b < NUM_BUTTONS; b++) {
        _buttons[b]->scan();
    }

    _wheel->scan();

    for (uint8_t l = 0; l < NUM_LIGHTS; l++) {
        _lights[l]->tick();
    }

    x_move = x_direction.read_action();
    y_move = y_direction.read_action();
    if (x_move != 0 || y_move != 0) {
        Mouse.move(x_move, y_move, 0);
    }

    delay(1);
}
