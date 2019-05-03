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

#include "timer.h"

inline uint8_t readSwitch() {
  return 0x7 - ((digitalRead(CH3) << 2) | (digitalRead(CH2) << 1) | digitalRead(CH1));
}

void set_and_wait(uint8_t band, uint8_t menu_pos) {
  int16_t rssiB = 0, rssiA = 0;
  u8 activeReceiver;

#ifdef USE_DIVERSITY
  u8 diversityTargetReceiver;
  Timer diversityHysteresisTimer = Timer(DIVERSITY_HYSTERESIS_PERIOD);
#endif
  Timer rssiStableTimer = Timer(RSSI_STABLE_PERIOD);
  
  uint8_t last_post_switch = readSwitch();

  //no more RAM at this point :( lets consume less...
  TV.end();
  TV.begin(PAL, D_COL / 2, D_ROW / 2);
  TV.select_font(font4x6);
  TV.printPGM(0, 10, PSTR("PLEASE\nWAIT..."));
  
#ifdef USE_DIVERSITY
  
  rx5808B.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
  SELECT_B;
  activeReceiver = RX_B;
  diversityTargetReceiver = activeReceiver;
  
#else // !USE_DIVERSITY
  SELECT_A;
  activeReceiver = RX_A;
#endif // !USE_DIVERSITY
  
  rx5808A.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
  
  //save band and freq as "last used"
  EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos); //freq id
  EEPROM.write(EEPROM_ADDR_LAST_BAND_ID, band); //channel name
  
  //MAIN LOOP - change channel and log
  while (1) {

#ifdef USE_DIVERSITY

    if (rssiStableTimer.hasTicked()) {
      //rx5808A.getCurrentRSSI(); // Fake read to let ADC settle.
      rssiA = rx5808A.getCurrentRSSI();
      rssiA = constrain(map(rssiA, rx5808A.getRssiMin(), rx5808A.getRssiMax(), 0, 100), 0, 100);
  
      //rx5808B.getCurrentRSSI(); // Fake read to let ADC settle.
      rssiB = rx5808B.getCurrentRSSI();
      rssiB = constrain(map(rssiB, rx5808B.getRssiMin(), rx5808B.getRssiMax(), 0, 100), 0, 100);
  
      int8_t rssiDiff = (int8_t) rssiA - (int8_t) rssiB;
      uint8_t rssiDiffAbs = abs(rssiDiff);
      u8 currentBestReceiver = activeReceiver;
      
      if (rssiDiff > 0) {
        currentBestReceiver = RX_A;
      } else if (rssiDiff < 0) {
        currentBestReceiver = RX_B;
      }
  
      u8 nextReceiver = activeReceiver;
      if (rssiDiffAbs >= DIVERSITY_HYSTERESIS) {
        if (currentBestReceiver == diversityTargetReceiver) {
          if (diversityHysteresisTimer.hasTicked()) {
            nextReceiver = diversityTargetReceiver;
          }
        } else {
          diversityTargetReceiver = currentBestReceiver;
          diversityHysteresisTimer.reset();
        }
      } else {
        diversityHysteresisTimer.reset();
      }

      if (nextReceiver != activeReceiver) {
        if (nextReceiver == RX_B) {
          SELECT_B;
          activeReceiver = RX_B;
        } else {
          SELECT_A;
          activeReceiver = RX_A;
        }
      }
    }
    
#endif // USE_DIVERSITY
    
    menu_pos = readSwitch();
    if (last_post_switch != menu_pos) { //something changed by user
      last_post_switch = menu_pos;
      
      int i = 0;
      TV.clear_screen();
      for (i = 0; i < 8; i++) {
        TV.print(0, i * 6, pgm_read_byte_near(channelNames + (8 * band) + i), HEX); //channel freq
        TV.print(10, i * 6, pgm_read_word_near(channelFreqTable + (8 * band) + i), DEC); //channel name
      }
      TV.draw_rect(30, menu_pos * 6 , 5, 5,  WHITE, INVERT); //current selection
      SELECT_OSD;
      TV.delay(1000);
      
#ifdef USE_DIVERSITY
      rx5808B.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
      SELECT_B;
      activeReceiver = RX_B;
#else // !USE_DIVERSITY
      SELECT_A;
      activeReceiver = RX_A;
#endif // !USE_DIVERSITY
      
      rx5808A.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq

      EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos);

      rssiStableTimer.reset();
    }

  } //end of loop

}
