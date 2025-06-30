/*!
 *  @file Adafruit_PCF8574.cpp
 *
 * 	I2C Driver for ESP32-S3 QTPy based audio player
 *
 * 	This is a library for an  ESP32-S3 based audio player
 * 	using Adafruit ESP32-S3 QT-Py and the Audio BFF board
 *
 * 	@section license License
 *
 * 	BSD (see license.txt)
 *
 * 	@section  HISTORY
 *
 *     v1.0 - First release
 */

#include "I2C_SoundBoard.h"

/*!
 *    @brief  Instantiates a new I2C_SoundBoard class
 */
I2C_SoundBoard::I2C_SoundBoard(void) {
  last_status_response = 0;
  curr_volume = 5;
}


/*!
 *    @brief  Sets up the hardware and initializes I2C
 *    @param  i2c_address
 *            The I2C address to be used.
 *    @param  wire
 *            The Wire object to be used for I2C connections.
 *    @return True if initialization was successful, otherwise false.
 */
bool I2C_SoundBoard::begin(uint8_t i2c_address, TwoWire *wire) {
  
  if (i2c_dev)
    delete i2c_dev;
  
  i2c_dev = new Adafruit_I2CDevice(i2c_address, wire);
  
  if (!i2c_dev->begin())
    return false;

  return true;
}

bool I2C_SoundBoard::playSound( char* filename, int channel, bool repeat) {
    char _buffer[256] = {};

    if(!repeat) _buffer[0] = 'T';
    else  _buffer[0] = 'R';

    if( channel >= 0 && channel <= 3 )
        _buffer[1] = '0' + channel;
    else
        return false;    
        
    _buffer[2] = ' '; 

    strncpy( (char*)(_buffer+3), filename, 253 );   

    Serial.print( "Sent: " );
    Serial.println( _buffer );

    return i2c_dev->write( (const uint8_t*)_buffer, strlen(_buffer), true);

}

bool I2C_SoundBoard::stopSound( int channel ) {
    char _buffer[4] = {};

    _buffer[0] = 'S';
    
    if( channel >= 0 && channel <= 3 )
        _buffer[1] = '0' + channel;
    else
        return false;    
        

    Serial.print( "Sent: " );
    Serial.println( _buffer );
            
    return i2c_dev->write( (const uint8_t*)_buffer, strlen(_buffer), true);
}

bool I2C_SoundBoard::setVolume( int level ) {
  char _buffer[4] = {};
  if( level < 0 ) level = 0;
  if( level > 9 ) level = 9;

  sprintf( _buffer, "V%n", level );
  return i2c_dev->write( (const uint8_t*)_buffer, strlen(_buffer), true);
}

bool I2C_SoundBoard::volumeUp () {
  if( curr_volume < 9 ) curr_volume++;
  return i2c_dev->write( (const uint8_t*)"V+", 2, true);
}

bool I2C_SoundBoard::volumeDn () {
  if( curr_volume > 0 ) curr_volume--;
  return i2c_dev->write( (const uint8_t*)"V-", 2, true);
}

int I2C_SoundBoard::getVolume() {
  return curr_volume;
}

int I2C_SoundBoard::getLastStatusValue() {
  return (last_status_response & 0x0F);
}

bool I2C_SoundBoard::getStatus( int channel ) {
    char _buffer[5] = {};

    memset( (uint8_t*)_buffer, 0, 5 );
    if( true == i2c_dev->read( (uint8_t*)_buffer, 4, true) ) {        
        Serial.print( "received: " );
        Serial.println( _buffer );    
        
        if( !_buffer[0] == 'N' ){
          last_status_response = 0;
          return false;
        } else {
          
          curr_volume = (uint8_t)(_buffer[1] - '0') & 0x0F;
          
          uint8_t stt = (uint8_t)(_buffer[2] - '0') & 0x0F;
          last_status_response = stt;  
          
          if( channel == 0 ) return true;         
          if( channel == 1 ) return ( stt | 0x01 == 1 );
          if( channel == 2 ) return ( stt | 0x02 == 2 ); 
          if( channel == 3 ) return ( stt | 0x04 == 4 ); 
        }
    }
    
    return false;    
}
