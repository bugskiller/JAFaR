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

inline uint8_t readSwitch() {
  return 0x7 - ((digitalRead(CH3) << 2) | (digitalRead(CH2) << 1) | digitalRead(CH1));
}

void set_and_wait(uint8_t band, uint8_t menu_pos) {
  int16_t rssi_b = 0, rssi_a = 0, rssi_b_norm = 0, rssi_a_norm = 0, prev_rssi_b_norm = 0, prev_rssi_a_norm = 0, global_max_rssi;
  u8 current_rx;
  uint8_t last_post_switch = readSwitch();

  //no more RAM at this point :( lets consume less...
  TV.end();
  TV.begin(PAL, D_COL / 2, D_ROW / 2);
  TV.select_font(font4x6);
  TV.printPGM(0, 10, PSTR("PLEASE\nWAIT..."));
  
#ifdef USE_DIVERSITY
  
  rx5808B.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
  SELECT_B;
  current_rx = RX_B;

#else // USE_DIVERSITY
  SELECT_A;
  current_rx = RX_A;
#endif // USE_DIVERSITY

  rx5808A.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
  
  //save band and freq as "last used"
  EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos); //freq id
  EEPROM.write(EEPROM_ADDR_LAST_BAND_ID, band); //channel name

#ifdef USE_DIVERSITY
  global_max_rssi = max(rx5808A.getRssiMax(), rx5808B.getRssiMax());
#endif // USE_DIVERSITY
  
  //MAIN LOOP - change channel and log
  while (1) {

#ifdef USE_DIVERSITY
    
    rssi_a = rx5808A.getCurrentRSSI();
    
    rssi_a_norm = constrain(rssi_a, rx5808A.getRssiMin(), rx5808A.getRssiMax());
    rssi_a_norm = map(rssi_a_norm, rx5808A.getRssiMin(), rx5808A.getRssiMax(), 1, global_max_rssi);

    rssi_b = rx5808B.getCurrentRSSI();
    
    rssi_b_norm = constrain(rssi_b, rx5808B.getRssiMin(), rx5808B.getRssiMax());
    rssi_b_norm = map(rssi_b_norm, rx5808B.getRssiMin(), rx5808B.getRssiMax(), 1, global_max_rssi);

#ifdef DISABLE_FILTERING
    
    int16_t rssi_b_norm_filt  = (rssi_b_norm + prev_rssi_b_norm * 3) / 4;
    int16_t rssi_a_norm_filt  = (rssi_a_norm + prev_rssi_a_norm * 3) / 4;
    
#else // DISABLE_FILTERING

    //filter... thanks to A*MORALE!
    //alpha * (current-previous) / 2^10 + previous
    //alpha = dt/(dt+1/(2*PI *fc)) -> (0.0002 / (0.0002 + 1.0 / (2.0 * 3.1416 * 10))) = 01241041672 * 2^11 -> 25
    //dt = 200us
    //fc = 8HZ
    //floating point conversion 10 bit > shift 2^10 -> 1024 
#define ALPHA 25
     int16_t rssi_b_norm_filt = ((ALPHA * (rssi_b_norm - prev_rssi_b_norm)) / 1024) + prev_rssi_b_norm;
     int16_t rssi_a_norm_filt = ((ALPHA * (rssi_a_norm - prev_rssi_a_norm)) / 1024) + prev_rssi_a_norm;
     
#endif // DISABLE_FILTERING
    
    prev_rssi_b_norm = rssi_b_norm_filt;
    prev_rssi_a_norm = rssi_a_norm_filt;
    
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
      current_rx = RX_B;
#else // USE_DIVERSITY
      SELECT_A;
      current_rx = RX_A;
#endif // USE_DIVERSITY
      rx5808A.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq

      EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos);
    }

#ifdef USE_DIVERSITY
    if (current_rx == RX_B && rssi_a_norm_filt > rssi_b_norm_filt + RX_HYST) {
      SELECT_A;
      current_rx = RX_A;
    }

    if (current_rx == RX_A && rssi_b_norm_filt > rssi_a_norm_filt + RX_HYST) {
      SELECT_B;
      current_rx = RX_B;
    }
#endif // USE_DIVERSITY

  } //end of loop

}
