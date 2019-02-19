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

RX5808 rx5808A(rssiA, SPI_CSA);
#ifdef USE_DIVERSITY
RX5808 rx5808B(rssiB, SPI_CSB);
#endif

uint8_t last_post_switch, flag_first_pos,  in_mainmenu, menu_band, menu_pos;
float timer;
uint16_t last_used_freq, last_used_band, last_used_freq_id;

#include <TVout.h>
#include <fontALL.h>

TVout TV;

void inline jafar_delay(const uint16_t __delay) {
  TV.delay(__delay);
}

//////********* SETUP ************////////////////////
void setup() {
  //video out init
  pinMode(SW_CTRL1, OUTPUT);
  pinMode(SW_CTRL2, OUTPUT);
  SELECT_OSD;

  //initialize SPI
  pinMode(spiDataPin, OUTPUT);
  pinMode(spiClockPin, OUTPUT);

  //RX module init
  rx5808A.init(false); // false = not channel B

#ifdef USE_DIVERSITY
  // Init the second module
  rx5808B.init(true); // true = is channel B
#endif
  
  osd_init();
  
  flag_first_pos = readSwitch();
  last_post_switch = 0;

  in_mainmenu = 1;
  timer = TIMER_INIT_VALUE;

  //load last used values
  last_used_band = EEPROM.read(EEPROM_ADDR_LAST_BAND_ID); //channel name
  last_used_freq_id = EEPROM.read(EEPROM_ADDR_LAST_FREQ_ID);
  last_used_freq = pgm_read_word_near(channelFreqTable + (8 * last_used_band) + last_used_freq_id); //freq

  _init_selection = readSwitch();
}

void autoscan() {
  last_post_switch = -1; //force first draw
  timer = TIMER_INIT_VALUE;
  rx5808A.scan(); //refresh RSSI
  rx5808A.compute_top8();

  while (timer) {
    menu_pos = readSwitch();

    if (menu_pos != last_post_switch) { //user moving
      last_post_switch = menu_pos;
      timer = TIMER_INIT_VALUE;
    }

    osd_autoscan();

    jafar_delay(LOOPTIME);
    timer -= (LOOPTIME / 1000.0);
  }

  set_and_wait((rx5808A.getfrom_top8(menu_pos) & 0b11111000) / 8, rx5808A.getfrom_top8(menu_pos) & 0b111);
}

void scanner_mode() {
  osd_scanner();
  
  timer = TIMER_INIT_VALUE;
}

void loop(void) {
  uint8_t i;

  menu_pos = readSwitch();

  //force always the first menu item (last freq used)
  if (menu_pos == flag_first_pos) {
    menu_pos = _init_selection = 0;
  }

  //new user selection
  if (menu_pos != last_post_switch) {
    last_post_switch = menu_pos;
    flag_first_pos = 0;
    timer = TIMER_INIT_VALUE;
  }
  else {
    timer -= (LOOPTIME / 1000.0);
  }
  
  if (timer <= 0) { //end of time for selection

    if (in_mainmenu) { //switch from menu to submenu (band -> frequency)
      
      if (menu_pos == compute_position(LAST_USED_POS)) { //LAST USED
        set_and_wait(last_used_band, last_used_freq_id);
      }
      else if (menu_pos == compute_position(SCANNER_POS)) { //SCANNER
        scanner_mode();
      }
      else if (menu_pos == compute_position(AUTOSCAN_POS)) { //AUTOSCAN
        autoscan();
      }
      else {
        in_mainmenu = 0;
        menu_band = ((menu_pos - 1 - _init_selection + 8) % 8);
        timer = TIMER_INIT_VALUE;
      }
      
      jafar_delay(200);
    }
    else { //if in submenu
      //after selection of band AND freq by the user
      timer = 0;
      set_and_wait(menu_band, menu_pos);
    }
    
  } //timer
  
  //time still running
  if (in_mainmenu) { //on main menu
    osd_mainmenu(menu_pos) ;
  }
  else { //on submenu
    osd_submenu(menu_pos,  menu_band);
  }
  
  jafar_delay(LOOPTIME);
}
