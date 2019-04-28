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
const unsigned char step_pot_1 = A5,
                    step_pot_2 = A2,
                    log_pot_1  = A3,
                    log_pot_2  = A4,
                    joystick_x = A1,
                    joystick_y = A0,
                    joystick_b = 9;

// LCD screen
const unsigned char rs = 2,
                    rw = 3,
                    en = 4,
                    d4 = 5,
                    d5 = 6,
                    d6 = 7,
                    d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// DAC chip (LTC1658)
const unsigned char sck  = 10,
                    mosi = 11,
                    cs   = 12;
const unsigned char ser_dly = 2;
const unsigned long dac_setting = 0,
                    dac_limit = 0x3FFF, // maximum DAC output setting
                    semi = dac_limit / 4.7 / 12; // DAC steps per semitone

// Pattern mode variables
unsigned char possible_notes[13] = "CdDeEfgGaAbB",
              pattern_note[17]   = "CDCDCDCDCDCDCDCD",
              pattern_octave[17] = "0000000000000000";

unsigned char pattern_length = 16, // Number of steps in pattern.
              step = 0,
              cursor_position = 0,
              pattern_changed = 0;
unsigned long SPM = 400, // steps per minute
              SPM_max = 800,
              SPM_min = 15,
              pattern_start,
              step_time,
              note;

unsigned char stick_debouncing = 0,
              button_debouncing = 0;
unsigned long stick_db_start,
              button_db_start;
const unsigned long stick_db_delay = 100,
                    button_db_delay = 1000;

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

unsigned long current_time,
              start_time;

unsigned int select     = 0,
             horizontal = 0,
             vertical   = 0,
             step_1     = 0,
             step_2     = 0,
             log_1      = 0,
             log_2      = 0;


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

        delay(200);
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
        lcd.print("Log pot test");

        lcd.setCursor(0, 1);
        lcd.print("LP1:");
        lcd.print(analogRead(log_pot_1));

        lcd.setCursor(8, 1);
        lcd.print("LP2:");
        lcd.print(analogRead(log_pot_2));

        delay(200);
        current_time = millis();
        lcd.clear();
    }
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

        // Show option, and when button is pressed, execute selected option.
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
            lcd.print("Coming soon!");
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

unsigned int setTempo()
{
    log_2 = analogRead(log_pot_2);
    SPM = ((SPM_max - SPM_min) * log_2 / 1024) + SPM_min;
    return SPM;
}

void sequence()
{
    // Display the initial pattern.
    for (step=0; step<pattern_length; step++)
    {
        lcd.setCursor(step, 0);
        lcd.write(pattern_note[step]);
        lcd.setCursor(step, 1);
        lcd.write(pattern_octave[step]);
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
        log_2       = analogRead(log_pot_2);

        // Look for a button press-and-hold.
        if (select == 1 && button_debouncing == 0) // Just started the debounce.
        {
            button_db_start = millis();
            button_debouncing = 1;
        }
        else if (select == 1 && millis()-button_db_start > button_db_delay) // Debounce time has passed.
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
            button_debouncing = 0;
        }

        // Move the cursor if joystick moves horizontally.
        if (horizontal < 512 - js_threshold && stick_debouncing == 0)
        {
            stick_db_start = millis();
            stick_debouncing = 1;
        }
        else if (horizontal < 512 - js_threshold && millis() - button_db_start > stick_db_delay)
        {
            cursor_position++;
            if (cursor_position > pattern_length-1)
            {
                cursor_position = 0;
            }
        }
        else if (horizontal > 512 + js_threshold && millis() - button_db_start > stick_db_delay)
        {
            cursor_position--;
            if (cursor_position > pattern_length-1)
            {
                cursor_position = pattern_length;
            }
        }
        else if (horizontal < 512 + js_threshold && horizontal > 512 - js_threshold) // Signal dissappears.
        {
            stick_debouncing = 0;
        }
        lcd.setCursor(cursor_position, 0);
        /*
        // Change note if joystick moves vertically.
        if (vertical < 512 - js_threshold)
        {
            delay(motion_delay);
            pattern_note[cursor_position]++;
            pattern_changed = 1;
        }
        else if (vertical > 512 + js_threshold)
        {
            delay(motion_delay);
            pattern_note[cursor_position]--;
            pattern_changed = 1;

        }
        // Roll over to next octave.
        if (pattern_note[cursor_position] > 'G')
        {
            pattern_note[cursor_position] = 'A';
            pattern_octave[cursor_position] += 1;
        }
        else if (pattern_note[cursor_position] < 'A')
        {
            pattern_note[cursor_position] = 'G';
            pattern_octave[cursor_position] -= 1;
        }
        // Wrap around between first and last octave.
        if (pattern_octave[cursor_position] > '4')
        {
            pattern_octave[cursor_position] = '0';
        }
        else if (pattern_octave[cursor_position] < '0')
        {
            pattern_octave[cursor_position] = '4';
        }
        // After changing a note, update the display.
        if (pattern_changed == 1)
        {
            lcd.setCursor(cursor_position, 1);
            lcd.write(pattern_octave[cursor_position]);
            lcd.setCursor(cursor_position, 0);
            lcd.write(pattern_note[cursor_position]);
            pattern_changed = 0;
        }
        */

        // Set tempo by knob input.
        SPM = setTempo();

        // Move to the next step in the pattern if enough time has passed.
        step_time = 60000 / SPM;
        if (millis()-pattern_start > step_time*step)
        {
            // Calculate the DAC setting.
            note = (pattern_note[step] - 64 + ((pattern_octave[step] - 48) * 12)) * semi;
            // Output the note voltage.
            setDAC(note);
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
    Serial.begin(9600);

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
