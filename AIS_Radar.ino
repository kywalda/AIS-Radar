/*  AIS-Radar - display ais sender in a radar like screen
 *  Copyright (c) Arne Groh in spring 2023
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * ''Software''), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * This software is written and compiled in an Arduino environment for
 * a Raspberry Pi Pico RP2040. It is limited to show 11 ships/objects   
 * simultaneously (because of screen and ram size). 
 * You'll need also some hardware: 1 Pico, 1 TFT SPI display, 1 serial GPS module 
 * and 1 serial AIS receiver module. Both modules should send MNEA messages, 
 * which are decoded and used by the software.
 */
 
#define CENTER_X 160
#define CENTER_Y 160
#define TFT_SDA_READ
#define TOUCH_CS 26
// Width and height of sprite
#define WIDTH  320
#define HEIGHT 320

#define DBSIZE 12 // this includes the own position
#define COL_DIST 0.2  // minimal distance for collision alarm in miles

#include <TFT_eSPI.h> // Hardware-specific library from https://github.com/Bodmer/TFT_eSPI by Bodmer. Edit the "User_Setup.h" in library folder to meet display and pins!
/*
 * I used these GPIO pins of the Pico for a 3.5" TFT SPI 480x320 display: (for soldering and for "User_Setup.h" in library TFT_eSPI folder)
 *           PICO GPIO      Display
 * #define TFT_DC   2    -> DC/RS
 * #define TFT_CS   1    -> CS
 * #define TFT_MOSI 7    -> SDI(MOSI)
 * #define TFT_SCLK 6    -> SCK
 * #define TFT_RST  0    -> RESET
 * #define TFT_MISO 8    -> SDO(MISO)
 * 3.3V (OUT)            -> VCC,  and 
 * VBUS (+5V)            -> LED
 * GND                   -> GND
 */
  
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();                   // Create instance
TFT_eSprite spr = TFT_eSprite(&tft);


#include <UnixTime.h>   // from https://github.com/GyverLibs/UnixTime Alex Gyver <alex@alexgyver.ru>
#include "RPi_Pico_TimerInterrupt.h"  // from https://github.com/khoih-prog/RPI_PICO_TimerInterrupt  Written by Khoi Hoang
#define TIMER1_INTERVAL_MS  1000L // 1000 microsekunden = 1 millisek
RPI_PICO_Timer ITimer1(1);
UnixTime stamp(0);

#include <Arduino.h>
#include <AIS.h>  // from https://github.com/KimBP/AIS Kim Bøndergaarg <kim@fam-boendergaard.dk>

String me_line1, me_line2, utc_str, date_str, AISmsg, GPSmsg;
float fl_speed, lat_dec, lon_dec, track;
bool stringGPScomplete, stringAIScomplete, freeFound, rewrite, fix, dbfull, collision;
uint32_t unixTime;
char ais_char[40];
uint8_t nextFree, alarmlevel;

struct neighbors{
  unsigned int mmsi;
  float lati;
  float longi;
  float cog;
  float sog;
  String name;
  String callsign;
  uint32_t lastseen;
  unsigned int length;
  String type;
};

neighbors neighbor[DBSIZE + 1] = {};
String neighborline1[DBSIZE] = {};
String neighborline2[DBSIZE] = {};
uint16_t neighborcol[DBSIZE] = {};
uint16_t neighborbgcol[DBSIZE] = {};

const PROGMEM uint16_t color[16] =   {TFT_WHITE,    TFT_RED, TFT_YELLOW, TFT_GREEN,   TFT_ORANGE, TFT_CYAN,  TFT_PINK,  TFT_MAGENTA, TFT_GREENYELLOW, TFT_BLUE,   TFT_RED, TFT_YELLOW, TFT_GREEN,   TFT_ORANGE, TFT_CYAN,  TFT_PINK,};
const PROGMEM uint16_t bgcolor[16] = {TFT_BLACK,  TFT_WHITE, TFT_BLACK,  TFT_BLACK,   TFT_BLACK, TFT_BLACK, TFT_BLACK,   TFT_BLACK,    TFT_BLACK,     TFT_WHITE, TFT_WHITE, TFT_BLACK,  TFT_BLACK,   TFT_BLACK, TFT_BLACK, TFT_BLACK};
const PROGMEM String shiptype[20] = {"Wing", "SAR", "Tug", "Fishing", "Sailing", "Pleasure", "Diving", "Military", "Dredging", "Pilot", "Police", "Medical", "Tender", "H-Speed", "Special", "Passenger", "Cargo", "Tanker", "Other"};
const PROGMEM String messages[5] = {"okay", "no fix", "DB full", "COLLISION"};

// test AIS messages
//AIVDM[1] = "!AIVDM,1,1,,A,1CF4kT3000Nm`Kl@BHKJC@IH0000,0*37 \
//!AIVDM,1,1,,B,4028jOQur1laeNlMIL@<`8O00@JU,0*5F";

#include "functions.h"

void setup()  // Core 0 manages data-aquirement and most calculations
{
  ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS * 5000, TimerHandler1); // every 5000 msec
  Serial.begin(38400);
  
  // Setup UART0 (Serial1) with interrupt for GPS
  /*
   * I used a ublox NEO 6M module
   * it was configured with u-center software to send only 
   * RMC messages every second with 38400 baud
   * PICO         ublox
   * GPIO 16  ->  RX
   * GPIO 17  ->  TX
   * 3.3V(OUT)->  VCC
   * GND      ->  GND
   */
  uart_init(uart0, 38400);  // Serial1 this is connected to GPS module
  gpio_set_function(16, GPIO_FUNC_UART); // TX GPIO 16  ->  RX on GPS
  gpio_set_function(17, GPIO_FUNC_UART); // RX GPIO 17  ->  TX on GPS
  uart_set_hw_flow(uart0, false, false); // no HW controll CTS/RTS
  uart_set_format(uart0, 8, 1, UART_PARITY_NONE); // (UART_ID, DATA_BITS, STOP_BITS, PARITY)
  uart_set_fifo_enabled(uart0, false); // no FIFO
  irq_set_exclusive_handler(20, on_uart0_rx); // setup interrupt handler uart0->20 point to interrupt function
  irq_set_enabled(20, true); // enable interrupt handler uart0->20
  uart_set_irq_enables(uart0, true, false); // enable UART Interrupt RX only

  // Setup UART1 (Serial2) with interrupt for AIS
  /*
   * I used a "dAISy HAT - AIS Receiver"
   * from https://wegmatt.com/
   * PICO         dAISy
   * GPIO 20  ->  RX
   * GPIO 21  ->  TX 
   * VBUS(+5V)->  5V
   * GND      ->  GND
   */
  uart_init(uart1, 38400);  // Serial2 this is connected to dAISy
  gpio_set_function(20, GPIO_FUNC_UART); // TX GPIO 20
  gpio_set_function(21, GPIO_FUNC_UART); // RX GPIO 21
  uart_set_hw_flow(uart1, false, false); // no HW controll CTS/RTS
  uart_set_format(uart1, 8, 1, UART_PARITY_NONE); // (UART_ID, DATA_BITS, STOP_BITS, PARITY)
  uart_set_fifo_enabled(uart1, false); // no FIFO
  irq_set_exclusive_handler(21, on_uart1_rx); // setup interrupt handler uart1->21, point to interrupt function
  irq_set_enabled(21, true); // enable interrupt handler uart1->21
  uart_set_irq_enables(uart1, true, false); // enable UART Interrupt RX only
} // end Setup


void loop()
{
  if (stringAIScomplete)
  {
    stringAIScomplete = false;
    //Serial.println(AISmsg);
    String ais_str = getValue(AISmsg,',',5);
    ais_str.toCharArray(ais_char, 40);
    unpackAIS(ais_char);
    AISmsg = "";
    freeFound = false;
  }


  if (stringGPScomplete)
  {
    stringGPScomplete = false;
    //Serial.println(GPSmsg);
    unpackGPS(GPSmsg);
    GPSmsg = "";
  }

  // find next free struct for AIS objects
  for (uint8_t i=1; i <= DBSIZE; i++){
    if ((neighbor[i].mmsi == 0) && (!freeFound)) {
      nextFree = i;
      dbfull = (i == DBSIZE)?true:false; 
      freeFound = true;
      break;
    }
  }
  //Serial.println(nextFree);
    
} // end loop core 0

void setup1() // Core 1   manages the display
{
  // Initialise the TFT registers
  tft.init();
  tft.setRotation(1);

  // Create a sprite of defined size
  spr.createSprite(WIDTH, HEIGHT);
  spr.setColorDepth(8);

  // Clear the TFT screen to black
  tft.fillScreen(TFT_BLACK);
  // Draw right text field
  tft.fillRect(322, 0, 480, 320, TFT_WHITE);
  tft.drawLine(320, 31, 480, 31, TFT_BLACK);
  tft.drawLine(320, 279, 480, 279, TFT_BLACK);

  //demoData();
}

void loop1(void)
{
    spr.fillRect(0, 0, WIDTH, HEIGHT, TFT_BLACK); 

    spr.drawCircle(160, 160, 160,TFT_WHITE);
    spr.drawCircle(160, 160, 120,TFT_WHITE);
    spr.drawCircle(160, 160, 80,TFT_WHITE);
    spr.drawCircle(160, 160, 40,TFT_WHITE);
  
    spr.drawLine(160, 0, 160, 320, TFT_WHITE);
    spr.drawLine(0, 160, 320, 160, TFT_WHITE);
      
    spr.setTextColor(TFT_WHITE,TFT_BLACK);
    spr.drawCentreString("N", 160, -2,2);
    spr.drawCentreString("1", 200, 152,2);
    spr.drawCentreString("2", 240, 152,2);
    spr.drawCentreString("3", 280, 152,2);
    spr.drawCentreString("4", 317, 152,2);
    spr.drawCentreString(" nm ",280 , 0,2);
    spr.drawCentreString(utc_str + " UTC", 38 , 0, 2);
    spr.drawCentreString(date_str, 35, 306, 2);

    //  draw my position 
    draw_track(0, 0, fl_speed, track, TFT_WHITE, "*");

    // draw neighbors
    uint8_t j=0;
    for (uint8_t i=1; i < DBSIZE; i++) {  // create neighbor table data
      if (neighbor[i].mmsi != 0) {
        j++;
        float distance = CalcDistance(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi);
        int bearing = CalcBearing(lat_dec, lon_dec, neighbor[i].lati, neighbor[i].longi);
        neighborline1[j] = "";
        neighborline1[j] += String(i) + ":" + (String)neighbor[i].mmsi + ", " + (String)neighbor[i].type + "(" + (String)neighbor[i].length + "m)";
        neighborline2[j] = "";
        neighborline2[j] += (String)neighbor[i].name + ", D=" + (String)distance + "nm@"  + (String)bearing + "`";
        neighborcol[j] = color[i];
        neighborbgcol[j] = bgcolor[i];
        // draw map
        draw_track(distance, bearing, neighbor[i].sog, neighbor[i].cog, color[i], String(i));
        //Serial.println(neighborline1[j]);
      }
    } //  end for-loop 
    
    if (rewrite){   //  with a timeout of Timer1
      //  my pos, cog, sog
      tft.fillRect(322, 0, 480, 30, TFT_WHITE);
      tft.setTextColor(TFT_BLACK,TFT_WHITE); 
      tft.drawString(me_line1, 322, 1,2);
      tft.drawString(me_line2, 322, 15,2);

      //  list of ais objects
      j = 1; uint8_t elements = 0;
      for (uint8_t i=1; i < DBSIZE; i++) {
        if (neighborline1[i] != "") elements++;
      }
      // draw table
      for (uint8_t i=1; i <= elements; i++){ 
        tft.fillRect(322, 14 + (j*20), 480 - 322, 19, neighborbgcol[j]);
        tft.setTextColor(neighborcol[j],neighborbgcol[j]);
        tft.drawString(neighborline1[j], 323, 16 + (j*20),1);
        tft.drawString(neighborline2[j], 334, 24 + (j*20),1);
        tft.drawLine(320, 33 + (j*20), 480, 33 + (j*20), TFT_BLACK);
        //Serial.println(neighborline1[j]);
        j++;        
      }
      tft.fillRect(322, 14 + (j*20), 480 - 322, 19, TFT_WHITE); // in case of deletion of an dataset - clear next field
      tft.fillRect(322, 14 + ((j+1)*20), 480 - 322, 19, TFT_WHITE); // in case of deletion of two datasets - clear one more field

      //  message field
      if (alarmlevel > 0) {
        tft.fillRect(322, 280, 480 - 322, 40, TFT_RED);
        tft.setTextColor(TFT_WHITE,TFT_RED);
        tft.drawCentreString(messages[alarmlevel], 400, 290, 4); 
      } else {
        tft.fillRect(322, 280, 480 - 322, 40, TFT_GREEN);
        tft.setTextColor(TFT_BLACK,TFT_GREEN);
        tft.drawCentreString(messages[alarmlevel], 400, 290, 4);
      }
      
      rewrite = false;
    }   //  end rewite

    spr.pushSprite(0, 0);
    //delay(1);
}   // end loop1


bool TimerHandler1(struct repeating_timer *t) {  // every 5 seconds
  rewrite = true;
  messageMgr();
  clearUp();
  return true;
}

// uart0 RX interrupt handler this is GPS-receiver uBlox
void on_uart0_rx() {
    while (uart_is_readable(uart0)) {
        char ch = (char)uart_getc(uart0);
        if (ch == '\n') {
          stringGPScomplete = true;
          //rewrite = true;
        } else {
          GPSmsg += ch;
        }
    }   
}

// uart1 RX interrupt handler this is AIS-receiver dAISy
void on_uart1_rx() {
    while (uart_is_readable(uart1)) {
        char ch = (char)uart_getc(uart1);
        if (ch == '\n') {
          stringAIScomplete = true;
        } else {
          AISmsg += ch;
        }
    }      
}
