# I2C_Sound
I2C sound board, using Adafruit ESP32-S3 QTPy and Audio BFF, 

This is code to be run on an Adafruit ESP32-S3 Qt Py microcontroller board, connected to their Audio BFF I2S amplifier board.
The links are here: 
Esp32-S3 QT Py: https://www.adafruit.com/product/5426
Audio BFF: https://www.adafruit.com/product/5769?srsltid=AfmBOorctRdHagaswOJdZKHtMtZQuKDNMssrvKNy3UYo6TedYqpVtmBR

This software comes in 2 parts: The sketch that runs on the board itself, and then a library to connect and send requests to the board from another microcontroller.

The key take-aways for this configration are:  
   * 3 simultaneous channels of sound that can all play concurrently.
   * Any of these can be set to auto-repeat
   * A play sound request can be sent to first available idle channel, or pinned to a specific channel.

The sound files are uncompressed WAV files (working on adding others) and all are contained in the root directory of a MicroSD card inserted into the AudioBFF. The speaker should be a 3w 4ohm - 8 ohm speaker, connected by a micro molex plug. (See AudioBFF link above)

I wrote this for my Fallout 4 PipBoy 3000 project, as I was unsatisfied using the AdaFruit SoundFx board.

Start by including the library header file 

#include "I2C_SoundBoard.h";

Then, declare an instance of the class:  

//
// Address for the I2C controlled sound board.
//
#define I2C_DEV_ADDR 0x55
I2C_SoundBoard sound;


Then call begin in your startup function:

 sound.begin( I2C_DEV_ADDR, &Wire );

 In the remainder of your program, just make the calls as needed:

 sound.setVolume(6);
 sound.playSound( "radio4", 0, false );

 etc.
