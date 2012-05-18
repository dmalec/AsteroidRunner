// -------------------------------------------------------------------------------
// An Arduino sketch that implements a side-scrolling asteroid-dodging game.
//
// MIT license.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//
//      ******************************************************
//      Designed for the Adafruit RGB LCD Shield Kit
//      http://www.adafruit.com/products/716
//      or
//      Adafruit Negative RGB LCD Shield Kit
//      http://www.adafruit.com/products/714
//      ******************************************************
//
//
// --------------------------------------------------------------------------------
// Dependencies
// --------------------------------------------------------------------------------
// Adafruit Industries's RGB 16x2 LCD Shield library:
//       https://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library
// Adafruit Industries's MCP23017 I2C Port Expander library:
//       https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
// --------------------------------------------------------------------------------
#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>

//! The current state.
void (*state)() = NULL;

//! The state that the game was in prior to the current state.
void (*last_state)() = NULL;

//! The time in milliseconds since the last state change.
unsigned long last_state_change_time;

//! The current time in milliseconds since boot.
unsigned long time;

//! The bitmask of currently clicked buttons.
uint8_t clicked_buttons;

//! The array of asteroids.
uint8_t asteroids[40];

//! The amount of shield power left.
uint8_t shield_power;

//! The current wave / diffuculty level.
uint8_t wave;

//! The current score.
long score;

//! The column being generated.
uint8_t render_col = 16;

//! The player column.
uint8_t player_col = 0;

//! The player row.
uint8_t player_row = 0;

//! The last render timestamp.
long last_render = 0;

//! The LCD display object.
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

//! Enum of backlight colors.
enum BackLightColor { RED=0x1, YELLOW=0x3, GREEN=0x2, TEAL=0x6, BLUE=0x4, VIOLET=0x5, WHITE=0x7 };

//! Array of custom bitmap sprites.
/*!
  Custom sprites created using: http://www.quinapalus.com/hd44780udg.html
*/
byte sprites[7][8] =
{ { 0x00,0x0e,0x0e,0x00,0x00,0x00,0x00,0x00 }, // top asteroid
  { 0x00,0x00,0x00,0x00,0x00,0x0e,0x0e,0x00 }, // bottom asteroid
  { 0x00,0x0e,0x0e,0x00,0x00,0x0e,0x0e,0x00 }, // top & bottom asteroid

  { 0x10,0x1e,0x1f,0x0c,0x00,0x00,0x00,0x00 }, // top ship
  { 0x00,0x00,0x00,0x00,0x10,0x1e,0x1f,0x0c }, // bottom ship
  { 0x10,0x1e,0x1f,0x0c,0x00,0x0e,0x0e,0x00 }, // top ship / bottom asteroid
  { 0x00,0x0e,0x0e,0x00,0x10,0x1e,0x1f,0x0c }, // bottom ship / top asteroid
};

//! Index into the bitmap array for the top asteroid sprite.
const int TOP_ASTEROID_SPRITE_IDX = 0;

//! Index into the bitmap array for the bottom asteroid sprite.
const int BOTTOM_ASTEROID_SPRITE_IDX = 1;

//! Index into the bitmap array for the top & bottom asteroid sprite.
const int TOP_BOTTOM_ASTEROID_SPRITE_IDX = 2;

//! Index into the bitmap array for the top ship sprite.
const int TOP_SHIP_SPRITE_IDX = 3;

//! Index into the bitmap array for the bottom ship sprite.
const int BOTTOM_SHIP_SPRITE_IDX = 4;

//! Index into the bitmap array for the top ship / bottom asteroid sprite.
const int TOP_SHIP_BOTTOM_ASTEROID_SPRITE_IDX = 5;

//! Index into the bitmap array for the bottom ship / top asteroid sprite.
const int BOTTOM_SHIP_TOP_ASTEROID_SPRITE_IDX = 6;

//! Perform one time setup for the game and put the game in the splash screen state.
void setup() {
  // LCD has 16 columns & 2 rows
  lcd.begin(16, 2);
  
  // Define custom sprites
  lcd.createChar(TOP_ASTEROID_SPRITE_IDX, sprites[TOP_ASTEROID_SPRITE_IDX]);
  lcd.createChar(BOTTOM_ASTEROID_SPRITE_IDX, sprites[BOTTOM_ASTEROID_SPRITE_IDX]);
  lcd.createChar(TOP_BOTTOM_ASTEROID_SPRITE_IDX, sprites[TOP_BOTTOM_ASTEROID_SPRITE_IDX]);
  lcd.createChar(TOP_SHIP_SPRITE_IDX, sprites[TOP_SHIP_SPRITE_IDX]);
  lcd.createChar(BOTTOM_SHIP_SPRITE_IDX, sprites[BOTTOM_SHIP_SPRITE_IDX]);
  lcd.createChar(TOP_SHIP_BOTTOM_ASTEROID_SPRITE_IDX, sprites[TOP_SHIP_BOTTOM_ASTEROID_SPRITE_IDX]);
  lcd.createChar(BOTTOM_SHIP_TOP_ASTEROID_SPRITE_IDX, sprites[BOTTOM_SHIP_TOP_ASTEROID_SPRITE_IDX]);
  
  // Use Pin 0 to seed the random number generator
  randomSeed(analogRead(0));
  
  // Initial game state
  state = begin_splash_screen;  
}

//! Main loop of execution.
void loop() {
  time = millis();
  
  // Record time of state change so animations
  // know when to stop
  if (last_state != state) {
    last_state = state;
    last_state_change_time = time;
  }
  
  // Read in which buttons were clicked
  read_button_clicks();
  
  // Call current state function
  state();

  delay(10);
}

// -------------------------------------------------------------------------------
// Common functions
// -------------------------------------------------------------------------------

//! Return a bitmask of clicked buttons.
/*!
  Examine the bitmask of buttons which are currently pressed and compare against
  the bitmask of which buttons were pressed last time the function was called.
  If a button transitions from pressed to released, return it in the bitmask.

  \return the bitmask of clicked buttons
*/
void read_button_clicks() {
  static uint8_t last_buttons = 0;
  
  uint8_t buttons = lcd.readButtons();
  clicked_buttons = (last_buttons ^ buttons) & (~buttons);
  last_buttons = buttons;
}

//! Draw a standard screen for wave begin / end & game over
void draw_game_screen(uint8_t backlight, __FlashStringHelper *message, boolean print_wave) {
  lcd.clear();
  lcd.setBacklight(backlight);
  lcd.setCursor(0, 0);
  lcd.print(message);
  if (print_wave) {
    lcd.print(wave);
  }
  lcd.setCursor(0, 1);
  lcd.write(BOTTOM_SHIP_SPRITE_IDX);
  lcd.setCursor(15, 1);
  lcd.write(BOTTOM_SHIP_SPRITE_IDX);
  lcd.setCursor(2, 1);
  lcd.print(F("SCORE "));
  lcd.print(score);
}

// -------------------------------------------------------------------------------
// Game start states / functions
// -------------------------------------------------------------------------------

void begin_splash_screen() {
  lcd.clear();
  lcd.setBacklight(TEAL);
  lcd.print(F("ASTEROID  RUNNER"));
  
  state = animate_splash_screen;
}

void animate_splash_screen() {
  static boolean blink = true;
  static unsigned long last_blink_time;
  
  if (time - last_blink_time >= 1000) {
    lcd.setCursor(0, 1);
    if (blink) {
      lcd.write(0x7E);
      lcd.print(F(" PRESS SELECT "));
      lcd.write(0x7F);
    } else {
      lcd.print(F("                "));
    }
    blink = !blink;
    last_blink_time = time;
  }
  
  if (clicked_buttons & BUTTON_SELECT) {
    state = start_game;
  }
}

void start_game() {  
  // Reset game state
  score = 0;
  wave = 1;

  state = begin_wave;
}

// -------------------------------------------------------------------------------
// Wave Transition States
// -------------------------------------------------------------------------------

void begin_wave() {
  draw_game_screen(GREEN, F("BEGIN WAVE "), true);

  // Clear the array of asteroid values
  memset(asteroids, 0, 40);

  // Reset wave state.
  shield_power = 10;
  render_col = 16;
  player_col = 0;
  player_row = 0;
  last_render = 0;

  state = begin_wave_delay;
}

void begin_wave_delay() {
  if (time - last_state_change_time >= 3000) {
    lcd.clear();
    state = play_game;
  }
}

void end_wave() {
  score += shield_power * 10;
  
  draw_game_screen(GREEN, F("END WAVE "), true);

  state = end_wave_delay;
}

void end_wave_delay() {
  if (time - last_state_change_time >= 3000) {
    wave++;
    state = begin_wave;
  }
}

// -------------------------------------------------------------------------------
// Main Game Loop State
// -------------------------------------------------------------------------------

void play_game() {
  if (time - last_render >= 250) {
    if (shield_power > 6) {
      lcd.setBacklight(BLUE);
    } else if (shield_power > 3) {
      lcd.setBacklight(YELLOW);
    } else {
      lcd.setBacklight(RED);
    }
    
    render_col = (render_col < 39) ? render_col + 1 : 0;

    asteroids[render_col] = 0;
    for (int i=0; i<2; i++) {
      lcd.setCursor(render_col, i);
      uint8_t asteroid_type = 13 - wave;
      asteroid_type = (asteroid_type < 3) ? 3 : asteroid_type;

      switch (random(asteroid_type)) {
        case 0:
          lcd.write(TOP_ASTEROID_SPRITE_IDX);
          asteroids[render_col] |= (0x01 << (i * 2)); 
          break;
        case 1:
          lcd.write(BOTTOM_ASTEROID_SPRITE_IDX);
          asteroids[render_col] |= (0x02 << (i * 2)); 
          break;
        case 2:
          if (wave > 5) {
            lcd.write(TOP_BOTTOM_ASTEROID_SPRITE_IDX);
            asteroids[render_col] |= (0x03 << (i * 2)); 
          } else {
            lcd.print(' ');
          }
          break;
        default:
          lcd.print(' ');
          break;
      }
      
      score++;
    }  
    lcd.scrollDisplayLeft();

    player_col = (player_col < 39) ? player_col + 1 : 0;
    lcd.setCursor(player_col, player_row / 2);
    if (player_row % 2 == 0) {
      if (asteroids[player_col] & (0x02 << (player_row / 2 * 2))) {
        lcd.write(TOP_SHIP_BOTTOM_ASTEROID_SPRITE_IDX);
      } else {
        lcd.write(TOP_SHIP_SPRITE_IDX);
      }

      // Check for collisions.
      if (asteroids[player_col] & (0x01 << (player_row / 2 * 2))) {
        lcd.setBacklight(RED);

        if (shield_power == 0) {
          state = game_over;
        } else {
          shield_power--;
        }
      }
    } else {
      if (asteroids[player_col] & (0x01 << (player_row / 2 * 2))) {
        lcd.write(BOTTOM_SHIP_TOP_ASTEROID_SPRITE_IDX);
      } else {
        lcd.write(BOTTOM_SHIP_SPRITE_IDX);
      }

      // Check for collisions.
      if (asteroids[player_col] & (0x02 << (player_row / 2 * 2))) {
        lcd.setBacklight(RED);

        if (shield_power == 0) {
          state = game_over;
        } else {
          shield_power--;
        }
      }
    }

    last_render = time;
  }
  
  if (clicked_buttons & BUTTON_UP) {
    player_row = (player_row > 0) ? player_row - 1 : 0;
  }
  if (clicked_buttons & BUTTON_DOWN) {
    player_row = (player_row < 3) ? player_row + 1 : 3;
  }
  
  if (score % 500 == 0) {
    state = end_wave;
  }
}

// -------------------------------------------------------------------------------
// Game over states / functions
// -------------------------------------------------------------------------------

void game_over() {
  draw_game_screen(RED, F("GAME OVER"), false);

  state = game_over_delay;
}

void game_over_delay() {
  if (time - last_state_change_time >= 5000) {
    state = begin_splash_screen;
  }
}
