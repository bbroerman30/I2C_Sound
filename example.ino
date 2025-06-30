//
//  Example for using I2C wave player. This is from my PipBoy 3000 example (hence the file names) and running on my
//  test driver board (another ESP32-S3 QT Py board, with an AdaFruit 1" OLED display) 
//
//  This is included just to give you an example of my test code, and how to use the library.
//
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Wire.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "I2C_SoundBoard.h"

//
//  These are for the OLED display
//
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     5 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//
// This is for the NeoPixel on the QTPy controller
//
#define NUMPIXELS        1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

//
// Address for the I2C controlled sound board.
//
#define I2C_DEV_ADDR 0x55
I2C_SoundBoard sound;

// show text on the 1" OLED display
void showText( const char* text_to_show );

void setup()
{
  Serial.begin(115200);
  //while (!Serial) delay(10);
  Serial.println("Hello!");
  
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.cp437(true);         // Use full 256 char 'Code Page 437' font  
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  
#if defined(NEOPIXEL_POWER)
    // If this board has a power control pin, we must set it to output and high
    // in order to enable the NeoPixels. We put this in an #if defined so it can
    // be reused for other boards without compilation errors
    pinMode(NEOPIXEL_POWER, OUTPUT);
    digitalWrite(NEOPIXEL_POWER, HIGH);
#endif

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(60); // not so bright

  pixels.fill(0xFF0000);
  pixels.show();
 
  showText("");

  Wire1.begin();
  
  // Finally, turn the NeoPixel Green to show we're ready.
  pixels.fill(0x00FF00);
  pixels.show();


  // Initialize the sound interface.
  sound.begin( I2C_DEV_ADDR, &Wire1 );

  // Send the first channel background sound. This file plays for over 2 hours.
  showText( "Sent Radio1" );  
  sound.playSound( "radio4", 0, false );
  
  delay(2000); // Pause for 2 seconds  

  // Now, send the second layer sound file. This will auto-repeat until stopped, or the board is powered down.
  showText( "Sent Rads2" );  
  sound.playSound( "rads2", 2, true );  
}

void loop()
{
  char tmp_string[30];

  // Flash the neopixel so I know we're starting the loop.
  pixels.fill(0x00FF00);
  pixels.show();
  delay(5);
  pixels.fill(0x000000);
  pixels.show();
  delay(5);   

  // Send a short sound on layer 3 (this is a beep I use for low battery)
  showText( "Sent T3 lowbat" );
  sound.playSound( "lowbat", 3, false );  
  delay(500);     

  // Get and display the status of the sound board on the OLED display.
  if( sound.getStatus() ) {
    memset(tmp_string, 0, 30);
    sprintf( tmp_string, "Playing (V:%u  1:%s  2:%s  3:%s )", sound.getVolume(), ((sound.getLastStatusValue() & 0x01 == 0x01)?"Y":"n"),((sound.getLastStatusValue() & 0x02 == 0x02)?"Y":"n"), ((sound.getLastStatusValue() & 0x04 == 0x04)?"Y":"n") ); 
    showText( tmp_string );
  } else {
    showText( "Idle" );
  }

  // and sleep for a bit until the next iteration.
  delay(3000);   
}

void showText( const char* text_to_show ) {    
  display.clearDisplay();
  display.setCursor(0, 0);     // Start at top-left corner  
  display.print( text_to_show );
  display.display();
}
