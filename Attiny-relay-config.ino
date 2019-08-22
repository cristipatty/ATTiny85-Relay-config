#include <TinyWireM.h>  
#include <LiquidCrystal_I2C.h> 

// LCD display 16x2 i2c address
#define LCD 0x27

// Pin # definition (gpio, digital mode);
// note that pin#0 and pin#2 are used for I2C
// for LCD display
#define RELAY     1 // RELAY command and also onboard LED
#define BTN_PWR   3 // activated once at start-up, when powerd in USB mode
                    // not activated at start-up when powered (not USB mode)
#define BTN_UP    4 // not working when powered in USB mode.
                    // activated once on start-up, when powered (not USB mode)
#define BTN_DOWN  5 // !! chinese prank: pin#5 is [reset], not usable :-(
#define _MAX_PINS 6

LiquidCrystal_I2C lcd(LCD,16,2);  // set address & 16 chars / 2 lines

// the setup routine runs once when you press reset:
void setup() {          
  TinyWireM.begin();

  lcd.init();                           // initialize the lcd 
  lcd.backlight();                      // Print a message to the LCD.
  
  // initialize the digital pin as an output.
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  pinMode(BTN_UP, INPUT_PULLUP);
//  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_PWR, INPUT_PULLUP);

  initButtonHandling();

  lcdGoMenu(false, false);
  
  delay(500);
}

// the loop routine runs over and over again forever:
void loop() {
  handleButton(BTN_UP);
//  handleButton(BTN_DOWN);
  handleButton(BTN_PWR);
  handleLCD();
}

/**
 * ===========================================================
 * Configuration variables
 * 
 */
short configWld1 = 80;
short configBrk  = 40;
short configWld2 = 80;
short configMaxMs = 300;
byte configMinMs = 0;
byte configStepMs = 20;

short mainWld1 = configWld1;
short mainBrk  = configBrk;
short mainWld2 = configWld2;

/**
 * ===========================================================
 * LCD handling
 * test mode: show on lcd what button is pressed,
 * and clear LCD after a timeout
 */
char lcdText[2][17];
unsigned long lcdBlinkTs;
unsigned long lcdBlinkMs[2] = {250, 700};
bool isConfigMenu;

void handleLCD() {
  lcdHandleBlinkZone();
}

bool isBlinkOn = true;

// zone 0 = cursor at (4,0) next 4 chars
// zone 1 = cursor at (12,0) next 4 chars
// zone 2 = cursor at (4,1) next 4 chars
// zone 3 = cursor at (12,0) next 8 chars
// zone other value = stop blink
byte lcdBlinkZoneCrt = -1;
#define START_BLINK(ZONE) (0 <= ZONE < 4)
#define ZONE_POS_X(ZONE) (ZONE == 0 || ZONE == 2 ? 4 : ZONE == 1 ? 12 : 8)
#define ZONE_POS_Y(ZONE) (ZONE == 0 || ZONE == 1 ? 0 : 1)
// if true size is 8, else 4 - for blinkink off, number of characters
#define ZONE_OFF_STR(ZONE) (ZONE == 3 ? "        " : "    ")
void lcdHandleBlinkZone() {
  unsigned long ts = millis();
  if ((!START_BLINK(lcdBlinkZoneCrt) && !isBlinkOn) || 
      START_BLINK(lcdBlinkZoneCrt) && !isBlinkOn && ts - lcdBlinkTs > lcdBlinkMs[0]) {
    lcd.setCursor(ZONE_POS_X(lcdBlinkZoneCrt), ZONE_POS_Y(lcdBlinkZoneCrt));
    lcd.print(&lcdText[ZONE_POS_Y(lcdBlinkZoneCrt)][ZONE_POS_X(lcdBlinkZoneCrt)]);
    isBlinkOn = true;
    lcdBlinkTs = ts;
  } else if (START_BLINK(lcdBlinkZoneCrt) && isBlinkOn && ts - lcdBlinkTs > lcdBlinkMs[1]) {
      lcd.setCursor(ZONE_POS_X(lcdBlinkZoneCrt), ZONE_POS_Y(lcdBlinkZoneCrt));
      lcd.print(ZONE_OFF_STR(lcdBlinkZoneCrt));
      isBlinkOn = false;
      lcdBlinkTs = ts;
  }
}


void lcdBlinkZone(int zone) {
  lcdBlinkZoneCrt = zone;
}

void lcdGoMenu(bool menu, bool isSave) {
  if (isConfigMenu && !menu) {
    if (isSave) {
      mainWld1 = configWld1;
      mainBrk = configBrk;
      mainWld2 = configWld2;
    } else {
      configWld1 = mainWld1;
      configBrk = mainBrk;
      configWld2 = mainWld2;
    }
  }
  isConfigMenu = menu;

  lcd.setCursor(0, 0);
  sprintf(lcdText[0], "WLD1 %03d BRK %03d",
    (isConfigMenu ? configWld1 : mainWld1), (isConfigMenu ? configBrk : mainBrk));
  lcd.print(lcdText[0]);
  lcd.setCursor(0, 1);

  sprintf(lcdText[1], "WLD2 %03d %s", configWld2,
    (isConfigMenu ? (isSave ? "SAVE?  " : "CONFIG ") : "PWR GO!")
    );
  lcd.print(lcdText[1]);
}

/**
 * ===========================================================
 * Relay command handling
 */

/**
 * ===========================================================
 * BUTTONS handling
 * 
 * the variables store values in arrays - for all pins even if
 * we'll use only some of the pins as buttons (max number of
 * pins is defined by _MAX_PINS)
 */

// the current reading from the input pin
byte buttonState[_MAX_PINS];
bool buttonStateLong[_MAX_PINS];
// the previous reading from the input pin
byte lastButtonState[_MAX_PINS];
// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
// the last time the output pin was toggled
unsigned long lastDebounceTs[_MAX_PINS];
// the debounce time; increase if the output flickers
int debounceDelay = 100;
int longPressDelay = 1500;

#define BUTTON_UP HIGH
#define BUTTON_DOWN LOW

void initButtonHandling() {
  long unsigned _ts = millis();
  for (int i = 0; i<_MAX_PINS; i++) {
    buttonState[i] = BUTTON_UP;
    lastButtonState[i] = BUTTON_UP;
    lastDebounceTs[i] = _ts;
  }
}

#define ZONE_NEXT (lcdBlinkZoneCrt == 3 ? 0 : lcdBlinkZoneCrt + 1)
#define ZONE_IS_SAVE(X) (X == 3)
#define CONFIG_WELD_INC(X) (X + configStepMs > configMaxMs ? configMinMs : X + configStepMs)
void buttonPress(int pin, bool isLongPress) {
  if (!isConfigMenu && pin == BTN_PWR && !isLongPress) {
    // main menu, short press PWR - command relay
    digitalWrite(RELAY, HIGH);
    delay(configWld1);
    digitalWrite(RELAY, LOW);
    delay(configBrk);
    digitalWrite(RELAY, HIGH);
    delay(configWld2);
    digitalWrite(RELAY, LOW);
  } else if (!isConfigMenu && pin == BTN_UP && isLongPress) {
    // main menu, long press UP - enter config menu
    lcdGoMenu(true, false);
    lcdBlinkZone(0);
  } else if (isConfigMenu && pin == BTN_PWR && !isLongPress) {
    lcdGoMenu(true, ZONE_IS_SAVE(ZONE_NEXT));
    lcdBlinkZone(ZONE_NEXT);
  } else if (isConfigMenu && pin == BTN_PWR && isLongPress) {
    lcdBlinkZone(-1);
    // exit config menu, long press PWR - without saving
    // enter Main Menu
    lcdGoMenu(false, false);
  } else if (isConfigMenu && pin == BTN_UP && !isLongPress) {
    if (lcdBlinkZoneCrt == 0) {
      configWld1 = CONFIG_WELD_INC(configWld1);
      lcdGoMenu(true, false);
    } else if (lcdBlinkZoneCrt == 1) {
      configBrk = CONFIG_WELD_INC(configBrk);
      lcdGoMenu(true, false);
    } else if (lcdBlinkZoneCrt == 2) {
      configWld2 = CONFIG_WELD_INC(configWld2);
      lcdGoMenu(true, false);
    } else if (lcdBlinkZoneCrt == 3) {
      // save and exit config menu
      // enter Main Menu
      lcdGoMenu(false, true);
    }
  }
}

void handleButton(byte pin) {
  // read the state of the switch into a local variable:
  byte reading = digitalRead(pin);

  long unsigned _ts = millis();

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState[pin]) {
    // reset the debouncing timer
    lastDebounceTs[pin] = _ts;
  }

  int _lastStateMs = _ts - lastDebounceTs[pin];
  if (_lastStateMs > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state

    // if it's long-press
    if (!buttonStateLong[pin] && reading == BUTTON_DOWN && _lastStateMs > longPressDelay) {
      // process long-press
      buttonPress(pin, true);
      buttonStateLong[pin] = true;
    }

    // if the button state has changed:
    if (reading != buttonState[pin]) {
      buttonState[pin] = reading;

      // if the new button state is BUTTON_UP
      // then it's a short press (on release)
      if (buttonState[pin] == BUTTON_UP) {
        if (buttonStateLong[pin]) {
          buttonStateLong[pin] = false;
        } else {
          // process short-press
          buttonPress(pin, false);
        }
      }
    }
  }
  
  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState[pin] = reading;
}
