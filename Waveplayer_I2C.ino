// Waveplayer_I2C
//
// Brad Broerman, brad@broerman.net
// 
//
// Play WAV files, controlled through I2C requests.
//
// This is based on the AudioBFF example from Adafruit, re-written for the ESP32-S3 QT Py
// The default I2C address is 0x55 (Use the library I am including in this)
// This uses the SdFat library, and the Adafruit Waveplayer class (which was written for SdFat)
// Up to 3 simultaneous streams of sound can be played at the same time. 
// I consider layer 1 the base layer. If layer 2 is also playing, it will be louder than layer 1 (60/40). 
// If Layer 3 is also playing, it will be louder than the other 2.
// The main core has the file handling and I2C code. A separate task is running on the other core to pull samles
// from the buffers and send them to the I2S output (to the Audio BFF). 
// Commands are of the format T<x> <filename> to trigger a sound. <x> is 0-3 where 0 means first available, and 1-3 is a specific layer.
//                            R<x> <filename> to trigger a sound and keep repeating it.
//                            S<x> to stop the sounds (0 to stop all sounds, otherwise x identifies the layer)
//                            V+ to increase volume
//                            V- to decrease volume
//                            V{n} to set the volume, where {n} is 0-9.
// The default value for volume is 5 (This sounds best on the speakers I have, so that's where I'm starting)
//
// If you request data from the I2C interface, it will return either "N;" if not playing anything, else it will return "P<xx>" for Playing
// followed by 2 HEX nibbles. the first one is the volume leve 0-9, and the second is a bitmask for each of the 3 layers, with LSB being layer 1
//

#include <Arduino.h>
#include "Wire.h"
#include "SdFat.h"
#include <ESP_I2S.h>
#include <Adafruit_WavePlayer.h>

#if SD_FAT_TYPE == 0
SdFat sd;
File dir; 
File file, file2, file3;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 dir;
File32 file, file2, file3;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile dir;
ExFile file, file2, file3;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile dir;
FsFile file, file2, file3;
#else  // SD_FAT_TYPE
#error invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// SD/MMC card defines
#define SD_CS_PIN A0 // QT Py Audio BFF SD chip select

#define SPI_CLOCK SD_SCK_MHZ(50)

#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif  ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

// I2S GPIO pin numbers
#define pBCLK A3 // QT Py Audio BFF default BITCLOCK
#define pWS   A2 // QT Py Audio BFF default LRCLOCK
#define pDOUT A1 // QT Py Audio BFF default DATA
I2SClass            i2s;       // I2S peripheral is...
i2s_data_bit_width_t bps = I2S_DATA_BIT_WIDTH_16BIT;
i2s_mode_t mode = I2S_MODE_STD;
i2s_slot_mode_t slot = I2S_SLOT_MODE_STEREO;

volatile   float volume = 0.50;

// Tracking open & initial load.
uint32_t  rate, rate2, rate3;
wavStatus status, status2, status3;
  
// Variables for loading/playing the WAV file.
Adafruit_WavePlayer player(false, 16); // mono speaker, 16-bit out.
char playing_file_1[EXFAT_MAX_NAME_LENGTH + 1] = {};
volatile bool       playing   = false; // For syncing cores.
volatile bool       load      = false;
volatile bool       repeat    = false;

// Channel 2 (simultaneous with channel 1 above)
Adafruit_WavePlayer player2(false, 16); // mono speaker, 16-bit out.
char playing_file_2[EXFAT_MAX_NAME_LENGTH + 1] = {};
volatile bool       playing2   = false; // For syncing cores.
volatile bool       load2      = false;
volatile bool       repeat2    = false;

// Channel 3 (simultaneous with channel 1 & 2 above)
Adafruit_WavePlayer player3(false, 16); // mono speaker, 16-bit out.
char playing_file_3[EXFAT_MAX_NAME_LENGTH + 1] = {};
volatile bool       playing3   = false; // For syncing cores.
volatile bool       load3      = false;
volatile bool       repeat3    = false;

TaskHandle_t Core0SoundTask;

//
// For the I2C commands to be sent to the Loop from onReceive...
//
#define I2C_DEV_ADDR 0x55
volatile bool i2c_cmd_available = false;
char i2c_command[EXFAT_MAX_NAME_LENGTH + 1] = {};

//
// Handle incoming communications from the master ESP32.
//
void onRequest() {
  char resp[256];  
  
  memset(resp, '\0', 256 );
  
  if( !playing && !playing2 && !playing3 ) {
    sprintf( resp, "N;");
  } else {    
    uint8_t intvol = int(volume * 10 ) & 0x0F;
    uint8_t stat = (intvol<<4) | (playing?1:0) | (playing2?2:0) | (playing3?4:0);
    sprintf(resp, "P%X;", stat);
  }

  Wire1.write((const uint8_t*)resp, strlen(resp));
}
 
void onReceive(int numBytes) {
  byte dataBuffer[numBytes+1] = {};
  int i = 0;

  memset(dataBuffer, '\0', numBytes+1 );
  while (Wire1.available()) { // Check if data is available
    dataBuffer[i] = Wire1.read(); // Read a byte
    i++;
  }

  // Now, set the global so that loop can open and play the requested file (or close the file playing)
  if( false == i2c_cmd_available ) {
    memset(i2c_command, '\0', 256 );
    strncpy(i2c_command, (const char*)dataBuffer, numBytes);
    i2c_cmd_available = true;
  }
  
}

void setup() {
  // debug output at 115200 baud
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  Serial.print("Start Debug out...");

  // setup SD-card
  Serial.print("Initializing SD card...");
  if (!sd.begin(SD_CONFIG)) {
    Serial.println(" failed!");
    return;
  }

  if( dir.open("/") ) {
    Serial.println(" Opened root directory!");
  } else {
    Serial.println(" Failed to open root directory.");    
  }

  Serial.println(" Starting I2S.");
  i2s.setPins(pBCLK, pWS, pDOUT);

  Serial.println(" Starting player thread.");   
  xTaskCreatePinnedToCore(
        codeForCore0SoundTask,
        "Core 0 sounds",
        10000,
        NULL,
        1,
        &Core0SoundTask,
        0);

  // Set up to get commands from I2C.
  // onReceive gets commands (like 'play 9'
  // onRequest sends status (like 'playing 9 22' or 'not playing'
  Serial.println(" Starting I2C.");   
  Wire1.onReceive(onReceive);
  Wire1.onRequest(onRequest);
  Wire1.begin((uint8_t)I2C_DEV_ADDR);
}

void loop() {

  //
  // While in the loop (idle part) if we get a command to play a sound file, open it up and start it going.
  //
  if( true == i2c_cmd_available ) {
    checkAndProcessI2CCommand();
    i2c_cmd_available = false;
  }
   
  // Now, if we started something in the idle part of the main loop, we need to start I2S
  if( playing || playing2 || playing3 ) {

    // Start I2S
    if (i2s.begin(mode, rate, bps, slot)) {
    
      // If OK, turn on the nanopixel to show we're playing something.
      digitalWrite(LED_BUILTIN, HIGH);

      while (playing || playing2) {
        Serial.print(".");

        // Load buffers if needed.
        if (load || (status == WAV_LOAD)) {
          load   = false;
          status = player.read();          
          if (status == WAV_ERR_READ) {
            Serial.println("read error");
            playing = false;          
          }
        } // end load

        if (load2 || (status2 == WAV_LOAD)) {
          load2   = false;
          status2 = player2.read();          
          if (status2 == WAV_ERR_READ) {
            Serial.println("read error");
            playing2 = false;          
          }
        } // end load

        if (load3 || (status3 == WAV_LOAD)) {
          load3   = false;
          status3 = player3.read();          
          if (status3 == WAV_ERR_READ) {
            Serial.println("read error");
            playing3 = false;          
          }
        } // end load
        
        // If one of the 2 playing flags goes false, we've reached the end of that file. Close the file.
        if( !playing ){
          file.close();

          if( true == repeat && strlen(playing_file_1) > 0 ) {
            if (!file.open(playing_file_1, O_RDONLY)) {        
              Serial.println("Unable to load file.");        
            } else {
              status = player.start(file, &rate); 
              playing = load = true;
            }
          }
          
        }
        
        if( !playing2 ) {
          file2.close();

          if( true == repeat2 && strlen(playing_file_2) > 0 ) {
            if (!file2.open(playing_file_2, O_RDONLY)) {        
              Serial.println("Unable to load file.");        
            } else {
              status2 = player2.start(file2, &rate2); 
              playing2 = load2 = true;
            }
          }
          
        }

        if( !playing3 ) {
          file3.close();

          if( true == repeat3 && strlen(playing_file_3) > 0 ) {
            if (!file3.open(playing_file_3, O_RDONLY)) {        
              Serial.println("Unable to load file.");        
            } else {
              status3 = player3.start(file3, &rate3); 
              playing3 = load3 = true;
            }
          }
          
        }

        // Check for I2C command
        if( true == i2c_cmd_available ) {
          checkAndProcessI2CCommand();
          i2c_cmd_available = false;
        }
        
      } // end while playing

      // If we've exited the loop, then both files are finished. Go ahead and close them, and Idle the I2S.
      file.close();
      file2.close();
      file3.close();
      i2s.write((int16_t)0);
      i2s.write((int16_t)0);
      i2s.end();
      
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      Serial.println("Failed to initialize I2S!");
    } // end i2s
  } // end outer if playing         

  Serial.print(".");
   delay(10);
}

void checkAndProcessI2CCommand() {
  // Command T1 <filename> so for now, make everything from the 3rd character on the filename (appending a .wav).
  char fnbuff[256];
  char cmd[2] = {};

  cmd[0] = i2c_command[0];
  cmd[1] = i2c_command[1];

  //
  // Play a sound file.
  //
  if( cmd[0] == 'T' || cmd[0] == 'R' ) {
    
      memset( fnbuff,'\0', 256 );
      sprintf( fnbuff, "/%s.wav", (const char*)(i2c_command+3) );
    
      Serial.println( fnbuff );

      // If slot 1 is idle, play the incoming sound there...
      if(!playing || '1' == cmd[1] ) {
        
        repeat = false;
        playing = false;
        file.close();
        
        if (!file.open(fnbuff, O_RDONLY)) {        
          Serial.println("Unable to load file.");
        }

        if( cmd[0] == 'R') repeat = true;
        
        status = player.start(file, &rate);
        file.getName(playing_file_1,sizeof playing_file_1);
        if ((status == WAV_OK) || (status == WAV_LOAD)) {
           playing = load = true;
        }

      // If slot 2 is idle, play the incoming sound there...
      } else if( !playing2 || '2' == cmd[1] ) {
        
        repeat2 = false;
        playing2 = false;
        file2.close();

        if (!file2.open(fnbuff, O_RDONLY)) {        
          Serial.println("Unable to load file.");
        }

        if( cmd[0] == 'R') repeat2 = true;  
        
        status2 = player2.start(file2, &rate);
        file2.getName(playing_file_2,sizeof playing_file_2);
        if ((status2 == WAV_OK) || (status2 == WAV_LOAD)) {
           playing2 = load2 = true;
        }

      // If slot 3 is idle, play the incoming sound there...  
      } else if( !playing3 || '3' == cmd[1] ) {
        
        repeat3 = false;
        playing3 = false;
        file3.close();
        
        if (!file3.open(fnbuff, O_RDONLY)) {        
          Serial.println("Unable to load file.");
        }
        
        if( cmd[0] == 'R') repeat3 = true;  
        
        status3 = player3.start(file3, &rate);
        file3.getName(playing_file_3,sizeof playing_file_3);
        if ((status3 == WAV_OK) || (status3 == WAV_LOAD)) {
           playing3 = load3 = true;
        }
      } 

  //
  //  Stop playback on a specified slot.
  //
  } else if( cmd[0] == 'S' ) {
      if( cmd[1] == '1' ) {
          if( playing ) {
              playing = false;
              file.close();                    
          }
      } else if( cmd[1] == '2' ) {
          if( playing2 ) {
              playing2 = false;
              file2.close();                                        
          }
      } else if( cmd[1] == '3' ) {
          if( playing3 ) {
              playing3 = false;
              file3.close();                                                            
          }
      }
  } else if( cmd[0] == 'V' ) {
      if( cmd[1] == '-' ) {
        if( volume > 0.00 ) volume = volume - 10.0;        
      } else if( cmd[1] == '+' ) {
        if( volume < 90.00 ) volume = volume + 10.0;
      } else {
        switch( cmd[1] ) {
          case '0': volume = 0.00; break;
          case '1': volume = 0.10; break;
          case '2': volume = 0.20; break;
          case '3': volume = 0.30; break;
          case '4': volume = 0.40; break;
          case '5': volume = 0.50; break;
          case '6': volume = 0.60; break;
          case '7': volume = 0.70; break;
          case '8': volume = 0.80; break;
          case '9': volume = 0.90; break;
        }
      }
  }
}

void codeForCore0SoundTask(void *parameter) { 
  int16_t right, left;

  while(1) { // Keep this task running (simulating the loop1 from the original example)
  
    while(playing) {
      
      left = right = 0;

      wavSample sample;
      switch (player.nextSample(&sample)) {
       case WAV_LOAD:     
        load = true; // No break, pass through...
       case WAV_OK:
        right = (int32_t)sample.channel0 * volume;
        left = (int32_t)sample.channel1 * volume;        
        break;
       case WAV_EOF:
       case WAV_ERR_READ:     
        playing = load = false;      
      } // end switch

     if(playing2) {
        switch (player2.nextSample(&sample)) {
         case WAV_LOAD:     
          load2 = true; // No break, pass through...
         case WAV_OK:

          if(playing){
            right = (right * 0.40 ) + ( ( (int32_t)sample.channel0 * volume ) * 0.60);
            left = (left * 0.40 ) + ( ( (int32_t)sample.channel1 * volume ) * 0.60 );        
          } else {
            right = ( (int32_t)sample.channel0 * volume ) ;
            left =  ( (int32_t)sample.channel1 * volume ) ; 
          }
          
          break;
         case WAV_EOF:
         case WAV_ERR_READ:     
          playing2 = load2 = false;      
        } // end switch
     }

     if(playing3) {
        switch (player3.nextSample(&sample)) {
         case WAV_LOAD:     
          load3 = true; // No break, pass through...
         case WAV_OK:

          if( playing || playing2 ) {
            right = (right * 0.40 ) + ( ( (int32_t)sample.channel0 * volume ) * 0.60);
            left = (left * 0.40 ) + ( ( (int32_t)sample.channel1 * volume ) * 0.60 );        
          } else {
            right = ( (int32_t)sample.channel0 * volume ) ;
            left =  ( (int32_t)sample.channel1 * volume ) ;             
          }
                   
          break;
         case WAV_EOF:
         case WAV_ERR_READ:     
          playing3 = load3 = false;      
        } // end switch
     }

      if(playing || playing2 || playing3) {
        i2s.write((uint8_t *)&right, 2);
        i2s.write((uint8_t *)&left, 2);
      }

    }

    // Sleep for a bit so this core can do other things too.
    delay(5);
  }
}
