/*
 * Firmware for CV Sequencer
 *
 * The point of this program is to drive the operation of a sequencer which
 * provides control-voltage output to a synthesizer module.
 *
 */

#include<LiquidCrystal.h>
#include<SPI.h>

/************************************************
 *  Variable definitions                        *
 ************************************************/

// User inputs
const char step_pot_1 = A5,
           step_pot_2 = A2,
           log_pot_1  = A3,
           log_pot_2  = A4,
           joystick_x = A1,
           joystick_y = A0,
           joystick_b = 9;

// LCD screen
const char rs = 2,
           rw = 3,
           en = 4,
           d4 = 5,
           d5 = 6,
           d6 = 7,
           d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DAC chip (LTC1658)
const char sck  = 10,
           mosi = 11,
           cs   = 12;
const char ser_dly = 10;
const long dac_setting = 0; 
const int dac_limit = 0x3FFF, // maximum DAC output setting
          semi = dac_limit / 5.0 / 12; // DAC steps per semitone

// Pattern mode variables
unsigned char pattern_note[]   = "CdDeEFgGaAbBCCCC",
              pattern_octave[] = "2222222222223401";

unsigned char pattern_length = 16, // Number of steps in pattern.
              step = 0,
              cursor_position = 0;
unsigned short SPM = 400; // steps per minute
unsigned long pattern_start,
              step_time;

unsigned char debouncing = 0;
unsigned long debounce_start;
const unsigned long motion_delay = 300;

// Menu mode variables
unsigned char previous_state = 0,
              current_state  = 0;
const unsigned char option_pattern_mode  = 0,
                    option_step_pot_test = 1,
                    option_log_pot_test  = 2,
                    option_joystick_test = 3;
const unsigned long menu_delay = 300;

// General variables
const unsigned char pattern_mode = 0,
                    menu_mode    = 1;
unsigned char previous_mode = 1,
              current_mode  = 1,
              go = 1;

int current_time,
    start_time;

int select     = 0,
    horizontal = 0,
    vertical   = 0;

const unsigned int js_threshold = 10;


/************************************************
 * Function definitions                         *
 ************************************************/

void setDAC(unsigned long level)
{
    unsigned long pow_two;
    signed char i;
    level = level << 2;

    for (i=15; i>=0; i--)
    {
        pow_two = pow(2, i);
        if (level >= pow_two)
        {
            level -= pow_two;
            digitalWrite(mosi, 1);
        } else
        {
            digitalWrite(mosi, 0);
        }
        delay(ser_dly);
        digitalWrite(sck, 1);
        delay(ser_dly);
        digitalWrite(sck, 0);
        delay(ser_dly);
    }
    digitalWrite(cs, 0);
    delay(ser_dly);
    digitalWrite(cs, 1);
    delay(ser_dly);
    digitalWrite(cs, 0);
    delay(ser_dly);
}

void joystick_test()
{
    int test_time = 10000,
        start_time = millis(),
        current_time = millis();
    while (current_time - start_time < test_time)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Joystick test");

        lcd.setCursor(0, 1);
        lcd.print("X:");
        lcd.print(analogRead(joystick_x));

        lcd.setCursor(6, 1);
        lcd.print("Y:");
        lcd.print(analogRead(joystick_y));

        lcd.setCursor(12, 1);
        lcd.print("B:");
        lcd.print(digitalRead(joystick_b));

        delay(500);
        current_time = millis();
    }
    lcd.clear();
}

void step_pot_test()
{
    int test_time = 10000,
        start_time = millis(),
        current_time = millis();
    while (current_time - start_time < test_time)
    {
        lcd.clear();
        lcd.print("Step pot test");

        lcd.setCursor(0, 1);
        lcd.print("SP1:");
        lcd.print(analogRead(step_pot_1));

        lcd.setCursor(8, 1);
        lcd.print("SP2:");
        lcd.print(analogRead(step_pot_2));

        delay(200);
        current_time = millis();
    }
    lcd.clear();
}

void log_pot_test()
{
    int test_time = 10000,
        start_time = millis(),
        current_time = millis();
    while (current_time - start_time < test_time)
    {
        lcd.setCursor(0, 1);
        lcd.print("LP1:");
        lcd.print(analogRead(log_pot_1));

        lcd.setCursor(8, 1);
        lcd.print("LP2:");
        lcd.print(analogRead(log_pot_2));

        delay(200);
        current_time = millis();
    }
    lcd.clear();
}

void buttonDelay()
{
    char button = !digitalRead(joystick_b);
    while (button == 1)
    {
        button = !digitalRead(joystick_b);
        delay(100);
    }
}

void menu()
{
    go = 1;
    previous_state = 1;
    current_state = 0;
    while (go == 1)
    {
        // Only clear the display if something changes.
        if (current_state != previous_state) 
        {
            lcd.clear();
            previous_state = current_state;
        }

        // Display the menu item number.
        lcd.setCursor(0, 1);
        lcd.print(byte(current_state)); // Must be a byte, otherwise it prints a number rather than a character.

        // Read user inputs.
        select      = !digitalRead(joystick_b); // Button pulls low. Invert signal so that push -> 1.
        horizontal  = analogRead(joystick_x);
        vertical    = analogRead(joystick_y);

        // When button is pressed, execute selected option.
        if (current_state == option_joystick_test) // Test joystick input.
        {
            lcd.setCursor(0, 0);
            lcd.print("Joystick test");
            if (select == 1)
            {
                joystick_test();
            }
        }
        else if (current_state == option_step_pot_test) // Test 5-way switch inputs.
        {
            lcd.setCursor(0, 0);
            lcd.print("Step pot test");
            if (select == 1)
            {
                step_pot_test();
            }
        }
        else if (current_state == option_log_pot_test) // Test potentiometer inputs.
        {
            lcd.setCursor(0, 0);
            lcd.print("Log pot test");
            if (select == 1)
            {
                log_pot_test();
            }
        }
        else if (current_state == option_pattern_mode) // Change to pattern mode.
        {
            lcd.setCursor(0, 0);
            lcd.print("Pattern mode");
            if (select == 1)
            {
                current_mode = pattern_mode;
                buttonDelay();
                delay(500);
                go = 0;
            }
        }
        else // Otherwise, report empty menu spot.
        {
            lcd.setCursor(0, 0);
            lcd.print("no menu option");
        }

        // Change state if the joystick is pushed sideways.
        if (horizontal < 512 - js_threshold) {
            delay(menu_delay);
            current_state++;
            if (current_state > 10){
                current_state = 0;
            }
        }
        else if (horizontal > 512 + js_threshold) {
            delay(menu_delay);
            current_state--;
            if (current_state > 10){
                current_state = 10;
            }
        }
    }
}

void sequence()
{
    // Display the initial pattern.
    for (int i=0; i<pattern_length; i++)
    {
        lcd.setCursor(i, 0);
        lcd.write(pattern_note[i]);
        lcd.setCursor(i, 1);
        lcd.write(pattern_octave[i]);
    }

    go = 1;
    lcd.blink(); // Display the block-style cursor.
    pattern_start = millis();
    while (go == 1)
    {
        // Read user inputs.
        select      = !digitalRead(joystick_b); // Button pulls low. Invert signal so that push -> 1.
        horizontal  = analogRead(joystick_x);
        vertical    = analogRead(joystick_y);

        // Look for a button press-and-hold.
        if (select == 1 && debouncing == 0) // Just started the debounce.
        {
            debounce_start = millis();
            debouncing = 1;
        }
        else if (select == 1 && millis()-debounce_start > 1000) // Debounce time has passed.
        {
            current_mode = menu_mode;
            lcd.noBlink();
            lcd.clear();
            lcd.print("Menu");
            buttonDelay();
            go = 0;
        }
        else if (select == 0) // Signal dissappears.
        {
            debouncing = 0;
        }

        // Move the cursor if joystick moves horizontally.
        if (horizontal < 512 - js_threshold) {
            delay(motion_delay);
            cursor_position++;
            if (cursor_position > pattern_length){
                cursor_position = 0;
            }
        }
        else if (horizontal > 512 + js_threshold) {
            delay(motion_delay);
            cursor_position--;
            if (cursor_position > pattern_length){
                cursor_position = pattern_length;
            }
        }
        lcd.setCursor(cursor_position, 0);
        
        // Move to the next step in the pattern if enough time has passed.
        step_time = 60000 / SPM;
        if (millis()-pattern_start > step_time*step)
        {
            // Go to the next step.
            step++;
        }
        // At the end of the pattern, start over.
        if (step > pattern_length-1)
        {
            step = 0;
            pattern_start = millis();
        }
    }
}


/************************************************
 *  The main program                            *
 ************************************************/

void setup()
{
    // User input pin setup
    pinMode(step_pot_1, INPUT);
    pinMode(step_pot_2, INPUT);
    pinMode(log_pot_1,  INPUT);
    pinMode(log_pot_2,  INPUT);
    pinMode(joystick_x, INPUT);
    pinMode(joystick_y, INPUT);
    pinMode(joystick_b, INPUT);

    // DAC chip pin setup
    pinMode(sck,  OUTPUT);
    pinMode(mosi, OUTPUT);
    pinMode(cs,   OUTPUT);

    // We are only going to write to the LCD, so just set R/W low.
    pinMode(rw, OUTPUT);
    digitalWrite(rw, 0);
    // Set the LCD's number of columns and rows.
    lcd.begin(16, 2);

}


void loop()
{
    // Display either the menu or the pattern.
    if (current_mode == menu_mode)
    {
        menu();
    }
    else if (current_mode == pattern_mode)
    {
        sequence();
    }

    // Only update the display if something changes.
    if (current_mode != previous_mode)
    {
        lcd.clear();
        previous_mode = current_mode;
    }
}
