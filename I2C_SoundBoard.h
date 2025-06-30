/*!
 *  @file I2C_SoundBoard.h
 *
 * 	I2C Driver for ESP32-S3 QTPy based audio player
 *
 * 	This is a library for an  ESP32-S3 based audio player
 * 	using Adafruit ESP32-S3 QT-Py and the Audio BFF board
 *
 *	BSD license
 */

#ifndef _BRADB_I2C_SOUNDBOARD_H
#define _BRADB_I2C_SOUNDBOARD_H

#include "Arduino.h"
#include <Adafruit_I2CDevice.h>

#define PSOUNDBOARD_I2CADDR_DEFAULT 0x55 ///< PCF8574 default I2C address

/*!
 *    @brief  Class that stores state and functions for interacting with
 *            the PCF8574 I2C Expander
 */
class I2C_SoundBoard {
public:
  I2C_SoundBoard();
  bool begin(uint8_t i2c_addr = PSOUNDBOARD_I2CADDR_DEFAULT, TwoWire *wire = &Wire);

  bool playSound( char* filename, int channel = 0, bool repeat = false);
  bool stopSound( int channel = 0 );
  bool setVolume( int level = 5 );
  bool volumeUp(); // 9 steps of volume
  bool volumeDn();
  bool getStatus( int channel = 0 );
  int  getLastStatusValue();
  int  getVolume();

private:
  uint8_t last_status_response, curr_volume;
  Adafruit_I2CDevice *i2c_dev;
};

#endif
