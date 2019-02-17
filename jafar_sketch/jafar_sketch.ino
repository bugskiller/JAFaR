/*
  This file is part of Fatshark© goggle rx module project (JAFaR).

    JAFaR is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    JAFaR is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Nome-Programma.  If not, see <http://www.gnu.org/licenses/>.

    Copyright © 2016 Michele Martinelli
*/


#include <EEPROM.h>

#include "rx5808.h"
#include "const.h"

RX5808 rx5808(rssiA, SPI_CSA);
#ifdef USE_DUAL_CAL
RX5808 rx5808B(rssiB, SPI_CSB);
#endif

uint8_t last_post_switch, flag_first_pos,  in_mainmenu, menu_band, menu_pos;
float timer;
uint16_t last_used_freq, last_used_band, last_used_freq_id;

#ifdef USE_OLED

#include "U8glib.h"

#ifdef USE_I2C_OLED
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0 | U8G_I2C_OPT_NO_ACK | U8G_I2C_OPT_FAST); // Fast I2C / TWI
#else
U8GLIB_SSD1306_128X64 u8g(8, A1, A4, 11 , 13); //CLK, MOSI, CS, DC, RESET
#endif

char j_buf[80];

#endif
#ifdef USE_OSD

#include <TVout.h>
#include <fontALL.h>

TVout TV;

#endif //USE OSD

void inline jafar_delay(const uint16_t __delay) {
#ifdef USE_OSD
  TV.delay(__delay);
#else
  delay(__delay);
#endif
}

//////********* SETUP ************////////////////////
void setup() {

#ifdef STANDALONE
  pinMode(CH1, INPUT_PULLUP); //UP
  pinMode(CH2, INPUT_PULLUP); //ENTER
  pinMode(CH3, INPUT_PULLUP); //DOWN
#endif
  //menu_pos = 5;

  //video out init
  pinMode(SW_CTRL1, OUTPUT);
  pinMode(SW_CTRL2, OUTPUT);
  SELECT_OSD;

  //initialize SPI
  pinMode(spiDataPin, OUTPUT);
  pinMode(spiClockPin, OUTPUT);

  //RX module init
  rx5808.init(false); // false = not channel B
  //rx5808.calibration();

#ifdef USE_DUAL_CAL
  //init of the second module
  rx5808B.init(true); // true = is channel B
#endif

#ifdef USE_OLED
  oled_init();
#endif
#ifdef USE_OSD
  osd_init();
#endif

#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("HELLO WORLD\n");

  int i = 0;
  Serial.print("get rssi per band:");
  for (i = 0; i < NUM_BANDS; i++) {
    Serial.println(rx5808.getMaxValBand(i, 7), DEC);
  }
  Serial.println("");
#endif

  flag_first_pos = 0;
#ifdef FORCE_FIRST_MENU_ITEM
  flag_first_pos = readSwitch();
  last_post_switch = 0;
#else
  last_post_switch = -1; //init menu position
#endif

  in_mainmenu = 1;
  timer = TIMER_INIT_VALUE;

  //load last used values
  last_used_band = EEPROM.read(EEPROM_ADDR_LAST_BAND_ID); //channel name
  last_used_freq_id = EEPROM.read(EEPROM_ADDR_LAST_FREQ_ID);
  last_used_freq = pgm_read_word_near(channelFreqTable + (8 * last_used_band) + last_used_freq_id); //freq

#ifdef USE_I2C_OLED  //default - set the last freq
  rx5808.setFreq(last_used_freq); //set the last freq
#endif

  _init_selection = readSwitch();
}

void autoscan() {
  int reinit = 1; //only the first time, re-init the oled
  last_post_switch = -1; //force first draw
  timer = TIMER_INIT_VALUE;
  rx5808.scan(); //refresh RSSI
  rx5808.compute_top8();

  while (timer) {
    menu_pos = readSwitch();

    if (menu_pos != last_post_switch)  //user moving
      timer = TIMER_INIT_VALUE;

#ifdef USE_OLED
    oled_autoscan(reinit);
    reinit = 0;
#endif
#ifdef USE_OSD
    osd_autoscan();
#endif

    last_post_switch = menu_pos;

    jafar_delay(LOOPTIME);
    timer -= (LOOPTIME / 1000.0);
  }

  set_and_wait((rx5808.getfrom_top8(menu_pos) & 0b11111000) / 8, rx5808.getfrom_top8(menu_pos) & 0b111);
}

#ifdef USE_SCANNER
void scanner_mode() {

#ifdef USE_OLED
  oled_scanner();
#endif
#ifdef USE_OSD
  osd_scanner();
#endif

  timer = TIMER_INIT_VALUE;
}
#endif

void loop(void) {
  uint8_t i;

  menu_pos = readSwitch();

  //force always the first menu item (last freq used)
#ifdef FORCE_FIRST_MENU_ITEM
  if (flag_first_pos == menu_pos)
    menu_pos = _init_selection = 0;
#endif

  //new user selection
  if (last_post_switch != menu_pos) {
    flag_first_pos = 0;
    timer = TIMER_INIT_VALUE;
#ifdef STANDALONE //debounce
    jafar_delay(JAFARE_DEBOUCE_TIME);
#endif
#ifdef USE_I2C_OLED //changing freq every pression
    if (!in_mainmenu)
      rx5808.setFreq(pgm_read_word_near(channelFreqTable + (8 * menu_band) + menu_pos));
#endif
  }
#ifndef STANDALONE //no timer in standalone
  else {
    timer -= (LOOPTIME / 1000.0);
  }
#endif

  last_post_switch = menu_pos;

  if (timer <= 0) { //end of time for selection

    if (in_mainmenu) { //switch from menu to submenu (band -> frequency)
      if (menu_pos == compute_position(LAST_USED_POS)) //LAST USED
        set_and_wait(last_used_band, last_used_freq_id);

#ifdef USE_SCANNER
      else if (menu_pos == compute_position(SCANNER_POS)) //SCANNER
        scanner_mode();
#endif
      else if (menu_pos == compute_position(AUTOSCAN_POS)) //AUTOSCAN
        autoscan();
      else {
        in_mainmenu = 0;
        menu_band = ((menu_pos - 1 - _init_selection + 8) % 8);
#ifdef USE_I2C_OLED
        set_and_wait(menu_band, menu_pos);
#else
        timer = TIMER_INIT_VALUE;
#endif
      }

      jafar_delay(200);

    } else { //if in submenu
      //after selection of band AND freq by the user
      timer = 0;
      set_and_wait(menu_band, menu_pos);
    } //else
  } //timer

  //time still running
  if (in_mainmenu) { //on main menu
#ifdef USE_OLED
    oled_mainmenu(menu_pos);
#endif
#ifdef USE_OSD
    osd_mainmenu(menu_pos) ;
#endif
  } else { //on submenu
#ifdef USE_OLED
    oled_submenu(menu_pos,  menu_band);
#endif
#ifdef USE_OSD
    osd_submenu(menu_pos,  menu_band);
#endif
  }
  jafar_delay(LOOPTIME);
}

