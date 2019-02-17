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
#ifdef STANDALONE
  int but_up = digitalRead(CH1);
  int but_ent = digitalRead(CH2);
  int but_down = digitalRead(CH3);

  if (but_up == LOW && menu_pos < 7)
    menu_pos++;

  if (but_down == LOW && menu_pos > 0)
    menu_pos--;

  if (but_ent == LOW)
    timer = 0;

  return menu_pos;
#else
  return 0x7 - ((digitalRead(CH3) << 2) | (digitalRead(CH2) << 1) | digitalRead(CH1));
#endif
}

void set_and_wait(uint8_t band, uint8_t menu_pos) {
  int16_t rssi_b = 0, rssi_a = 0, rssi_b_norm = 0, rssi_a_norm = 0, prev_rssi_b_norm = 0, prev_rssi_a_norm = 0, global_max_rssi;
  u8 current_rx;
  uint8_t last_post_switch = readSwitch();

#ifdef USE_OSD
  //no more RAM at this point :( lets consume less...
  TV.end();
  TV.begin(PAL, D_COL / 2, D_ROW / 2);
  TV.select_font(font4x6);
  TV.printPGM(0, 10, PSTR("PLEASE\nWAIT..."));
#endif

#ifdef USE_DIVERSITY
 #ifndef USE_DUAL_CAL
  //init of the second module
  RX5808 rx5808B(rssiB, SPI_CSB);
  pinMode (rssiB, INPUT);
  pinMode (SPI_CSB, OUTPUT);
  rx5808B.setRSSIMinMax();
 #endif

  rx5808B.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
  SELECT_B;
  current_rx = RX_B;

#else
  SELECT_A;
  current_rx = RX_A;
#endif

  rx5808.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq

#ifdef DEBUG
  int i = 0;
  band = 2;
  menu_pos = 3;

  Serial.print("forcing freq: ");
  Serial.println(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos), DEC);
#endif

  //clear memory for log
#ifdef ENABLE_RSSILOG
  uint8_t sample = 0;
  long g_log_offset = 0;
  for (g_log_offset = 0 ; g_log_offset < EEPROM.length() / 2 ; g_log_offset++) {
    EEPROM.write(EEPROM_ADDR_START_LOG + g_log_offset, 0);
  }
  g_log_offset = 0;
#endif

  //save band and freq as "last used"
  EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos); //freq id
  EEPROM.write(EEPROM_ADDR_LAST_BAND_ID, band); //channel name

#ifdef USE_I2C_OLED //force screen refresh
  oled_submenu(menu_pos,  band);
#endif
  /* //TODO: this is the entry point for the re-init of oled
    U8GLIB_SSD1306_128X64 u8g2(8, A1, A4, 11 , 13); //CLK, MOSI, CS, DC, RESET
    u8g2.setFont(u8g_font_8x13);
    u8g2.firstPage();
    do {
      u8g2.drawStr( 0, 20, "JAFaR Project");
      u8g2.drawStr( 0, 35, "by MikyM0use");

      u8g2.setFont(u8g_font_6x10);
      sprintf (j_buf, "RSSI MIN %d", rssi_min); //Rssi min
      u8g2.drawStr(0, 50, j_buf);

      sprintf (j_buf, "RSSI MAX %d", rssi_max); //Rssi max
      u8g2.drawStr(0, 60, j_buf);
    } while ( u8g2.nextPage() );
    delay(4000);
  */

global_max_rssi = max(rx5808.getRssiMax(), rx5808B.getRssiMax());
  //MAIN LOOP - change channel and log
  while (1) {
#ifdef USE_DIVERSITY

#ifndef USE_DUAL_CAL
    global_max_rssi = max(rx5808.getRssiMax(), rx5808B.getRssiMax());
#endif
    rssi_a = rx5808.getCurrentRSSI();
#ifndef USE_DUAL_CAL
    if (rssi_a > rx5808.getRssiMax()) { //update to new max if needed
      rx5808.setRssiMax(rssi_a);
      global_max_rssi = max(rx5808.getRssiMax(), rx5808B.getRssiMax());
    }

    if (rssi_a < rx5808.getRssiMin()) //update to new min is needed
      rx5808.setRssiMin(rssi_a);
#endif

    rssi_a_norm = constrain(rssi_a, rx5808.getRssiMin(), rx5808.getRssiMax());
    rssi_a_norm = map(rssi_a_norm, rx5808.getRssiMin(), rx5808.getRssiMax(), 1, global_max_rssi);

    rssi_b = rx5808B.getCurrentRSSI();

#ifndef USE_DUAL_CAL
    if (rssi_b > rx5808B.getRssiMax()) { //this solve a bug when the goggles are powered on with no VTX around
      rx5808B.setRssiMax(rssi_b);
      global_max_rssi = max(rx5808.getRssiMax(), rx5808B.getRssiMax());
    }

    if (rssi_b < rx5808B.getRssiMin())
      rx5808B.setRssiMin(rssi_b);
#endif
    /*if ((abs(rx5808B.getRssiMax() - rx5808B.getRssiMin()) > 300) || (abs(rx5808B.getRssiMax() - rx5808B.getRssiMin()) < 50)) { //this solve a bug when the goggles are powered on with no VTX around
      rssi_b = 0;
      } else {

      }*/

    rssi_b_norm = constrain(rssi_b, rx5808B.getRssiMin(), rx5808B.getRssiMax());
    rssi_b_norm = map(rssi_b_norm, rx5808B.getRssiMin(), rx5808B.getRssiMax(), 1, global_max_rssi);

    //filter... thanks to A*MORALE!
    //alpha * (current-previous) / 2^10 + previous
    //alpha = dt/(dt+1/(2*PI *fc)) -> (0.0002 / (0.0002 + 1.0 / (2.0 * 3.1416 * 10))) = 01241041672 * 2^11 -> 25
    //dt = 200us
    //fc = 8HZ
    //floating point conversion 10 bit > shift 2^10 -> 1024
#define ALPHA 25
    int16_t rssi_b_norm_filt = ((ALPHA * (rssi_b_norm - prev_rssi_b_norm)) / 1024) + prev_rssi_b_norm;
    int16_t rssi_a_norm_filt = ((ALPHA * (rssi_a_norm - prev_rssi_a_norm)) / 1024) + prev_rssi_a_norm;


#ifdef DEBUG
    /* Serial.print("A min:");
      Serial.print(rx5808.getRssiMin(), DEC);
      Serial.print(" A max:");
      Serial.print(rx5808.getRssiMax(), DEC);
      Serial.print(" A norm:");
      Serial.print(rssi_a_norm, DEC);
      Serial.print(" B min:");
      Serial.print(rx5808B.getRssiMin(), DEC);
      Serial.print(" B max:");
      Serial.print(rx5808B.getRssiMax(), DEC);*/
    Serial.print(" B raw:");
    Serial.print(rssi_b, DEC);
    Serial.print(" B norm:");
    Serial.print(rssi_b_norm, DEC);
    Serial.print(" A raw:");
    Serial.print(rssi_a, DEC);
    Serial.print(" A norm:");
    Serial.print(rssi_a_norm, DEC);
    //Serial.print(" alpha-col:");
    //Serial.print(ALPHA * (rssi_b_norm - prev_rssi_b_norm), DEC);
    Serial.print(" Bfilt:");
    Serial.print(rssi_b_norm_filt, DEC);
    Serial.print(" Afilt:");
    Serial.println(rssi_a_norm_filt, DEC);

    //delay(100);
#endif
    prev_rssi_b_norm = rssi_b_norm_filt;
    prev_rssi_a_norm = rssi_a_norm_filt;
#ifdef ENABLE_RSSILOG
    //every loop cycle requires ~100ms (this is not true anymore -> TODO calculations again)
    //total memory available is 492B (512-20) and every sample is 2B -> 246 sample in total
    //if we take 2 sample per seconds we have 123 seconds of recording (~2 minutes)
    if (++sample >= 2) {
      sample = 0;

      //FORMAT IS XXXXXXXRYYYYYYYY (i.e. 7bit for RSSI_B - 1bit for RX used - 8bit for RSSI_A
      if (g_log_offset < EEPROM.length() / 2)
        EEPROM.put(EEPROM_ADDR_START_LOG + g_log_offset, ((uint16_t)(((rssi_b & 0xFE) | (current_rx & 0x1)) & 0xFF) << 8) | (rssi_a & 0xFF));

      g_log_offset += sizeof(uint16_t);
    }
#endif

#ifdef xDEBUG
    Serial.print("A: ");
    Serial.print(rssi_a, DEC);

    Serial.print("\tB: ");
    Serial.print(rssi_b, DEC);

    Serial.print("\twe are using: ");
    if (current_rx == RX_A) {
      Serial.print("\tA");
      Serial.print("\twe change at: ");
      Serial.println(rssi_a + RX_HYST, DEC);
    } else {
      Serial.print("\tB");
      Serial.print("\twe change at: ");
      Serial.println(rssi_b + RX_HYST, DEC);
    }


#endif //DEBUG

#endif //DIVERSITY

    menu_pos = readSwitch();

    if (last_post_switch != menu_pos) { //something changed by user

#ifdef USE_OSD
      int i = 0;
      TV.clear_screen();
      for (i = 0; i < 8; i++) {
        TV.print(0, i * 6, pgm_read_byte_near(channelNames + (8 * band) + i), HEX); //channel freq
        TV.print(10, i * 6, pgm_read_word_near(channelFreqTable + (8 * band) + i), DEC); //channel name
      }
      TV.draw_rect(30, menu_pos * 6 , 5, 5,  WHITE, INVERT); //current selection
      SELECT_OSD;
      TV.delay(1000);
#endif

#ifdef USE_DIVERSITY
      rx5808B.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq
      SELECT_B;
      current_rx = RX_B;
#else
      SELECT_A;
      current_rx = RX_A;
#endif
      rx5808.setFreq(pgm_read_word_near(channelFreqTable + (8 * band) + menu_pos)); //set the selected freq

      EEPROM.write(EEPROM_ADDR_LAST_FREQ_ID, menu_pos);

#ifdef USE_I2C_OLED
      oled_submenu(menu_pos,  band); //refresh screen
      jafar_delay(JAFARE_DEBOUCE_TIME);
#endif
#ifdef STANDALONE
      jafar_delay(JAFARE_DEBOUCE_TIME); //debounce
#endif

    }
    last_post_switch = menu_pos;

#ifdef USE_DIVERSITY
    if (current_rx == RX_B && rssi_a_norm_filt > rssi_b_norm_filt + RX_HYST) {
      SELECT_A;
      current_rx = RX_A;
    }

    if (current_rx == RX_A && rssi_b_norm_filt > rssi_a_norm_filt + RX_HYST) {
      SELECT_B;
      current_rx = RX_B;
    }
#endif

  } //end of loop

}


