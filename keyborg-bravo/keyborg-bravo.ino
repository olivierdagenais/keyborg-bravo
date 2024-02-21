// Arduino sketch to accessorize a keyboard.
// Copyright (C) 2022 Olivier Dagenais. All rights reserved.
// Licensed under the GNU General Public License. See LICENSE in the project
// root.

#include "Mouse.h"

typedef void (*callback_no_params)(void);

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

const int NUM_BUTTONS = 1;
Button *_buttons[NUM_BUTTONS];

const uint8_t MAX_BRIGHTNESS = 8;
AnalogLight *_redLight = new AnalogLight(3, MAX_BRIGHTNESS);
AnalogLight *_greenLight = new AnalogLight(6, MAX_BRIGHTNESS);
AnalogLight *_blueLight = new AnalogLight(9, MAX_BRIGHTNESS);
const int NUM_LIGHTS = 3;
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

bool _muteState = false;
void toggleMuteMomentary() {
    _muteState = !_muteState;
    notifyMuteState();
    _greenLight->blink(2);
}

void notifyMuteState() {
    Serial.println(_muteState ? 1 : 0);
}

void setup() {
    _buttons[0] = new Button(2, &toggleMuteMomentary, NULL);

    _lights[0] = _redLight;
    _lights[1] = _greenLight;
    _lights[2] = _blueLight;

    _redLight->blink(10);
    _blueLight->blink(1, 1000);

    /*
    _analogAxis[0] = new AnalogAxis(0, 1023, 0, 10, 2, 50, 0.1);
    _analogAxis[1] = new AnalogAxis(1, 1023, 0, 10, 2, 50, 0.1);
    // https://www.sparkfun.com/products/9426 (Thumb Slide Joystick) says:
    // "(...)you can expect a range of about 128 to 775 on each axis."
    _analogAxis[2] = new AnalogAxis(2, 128, 775, 6, 2, 20, 0.2);
    _analogAxis[3] = new AnalogAxis(3, 128, 775, 6, 2, 20, 0.2);
    */
    Serial.begin(9600);
    Serial.setTimeout(50 /* maximum milliseconds to wait for stream data */);
    notifyMuteState();
    Mouse.begin();
}

uint8_t _percent = 0;
String incomingString;
String oneOhOne = "101";

void loop() {
    for (uint8_t b = 0; b < NUM_BUTTONS; b++) {
        _buttons[b]->scan();
    }

    for (uint8_t l = 0; l < NUM_LIGHTS; l++) {
        _lights[l]->tick();
    }

    /*
    int16_t w = _analogAxis[0]->read(_percent);
    int16_t z = _analogAxis[1]->read(_percent);
    int16_t x = _analogAxis[3]->read(_percent);
    int16_t y = _analogAxis[2]->read(_percent);
    int16_t wheel = (z == 0 ? 0 : (z > 0 ? -1 : 1));
    if (_mouseActive && (x != 0 || y != 0 || z != 0)) {
        Mouse.move(x, y, wheel);
    }
    */

    if (Serial.available() > 0) {
        incomingString = Serial.readString();
        // As of 1.0, trim() modifies the String in place rather than returning a new one.
        incomingString.trim();
        unsigned int incomingLength = incomingString.length();
        char c = incomingString[0];
        switch (incomingLength)
        {
        case 1:
            switch (c)
            {
            case '0': // Red
                _redLight->blink(1, 1000);
                _greenLight->turnOff();
                _blueLight->turnOff();
                break;

            case '1': // Blue
                _redLight->turnOff();
                _greenLight->turnOff();
                _blueLight->blink(1, 1000);
                break;

            case '2': // Green
                _redLight->turnOff();
                _greenLight->blink(1, 1000);
                _blueLight->turnOff();
                break;

            case '3': // Purple
                _redLight->blink(1, 1000);
                _greenLight->turnOff();
                _blueLight->blink(1, 1000);
                break;

            case '4': // Yellow
                _redLight->blink(1, 1000);
                _greenLight->blink(1, 1000);
                _blueLight->turnOff();
                break;

            case '5': // White
                _redLight->blink(1, 1000);
                _greenLight->blink(1, 1000);
                _blueLight->blink(1, 1000);
                break;

            default:
                _redLight->turnOff();
                _greenLight->blink(5, 100);
                _blueLight->blink(5, 100);
                break;
            }
            break;
        case 3:
            if (incomingString[0] == '1'
                && incomingString[1] == '0'
                && incomingString[2] == '1'
                ) { // Flash green 3x
                _redLight->turnOff();
                _greenLight->blink(3, 250);
                _blueLight->turnOff();
            }
            break;

        default:
            break;
        }
    }

    _percent = (_percent + 1) % 100;
    delay(1);
}
