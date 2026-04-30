// GxEPD2_HelloWorld.ino by Jean-Marc Zingg
//
// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold80pt7b.h>
#include <Fonts/FreeSansBold60pt7b.h>

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"

// or select the display constructor line in one of the following files (old style):
#include "GxEPD2_display_selection.h"
#include "GxEPD2_display_selection_added.h"

// alternately you can copy the constructor from GxEPD2_display_selection.h or GxEPD2_display_selection_added.h to here
GxEPD2_BW<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(GxEPD2_154_Z90c(/*CS=D4*/ D2, /*DC=D5*/ D3, /*RST=D6*/ D6, /*BUSY=D7*/ D7));  // GDEH0154D67

void eKairnDisplayDigit(uint16_t rot, uint16_t poste, char mess[]) {
  display.init(115200, true, 2, false);  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.setRotation(rot);
  char Digit[4];
  display.setFont(&FreeSansBold80pt7b);
  if (poste == 0) {
    sprintf(Digit, "D");
  } else if (poste > 255) {
    sprintf(Digit, "A");
  } else {
    sprintf(Digit, "%d", poste);
    if (poste >= 100) display.setFont(&FreeSansBold60pt7b);
  }
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(Digit, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 3) - tby;
  display.setFullWindow();
  display.firstPage();
  do {
    // Display DIFIT
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(Digit);
    // Display Message
    display.setCursor(10, 190);
    display.setFont(&FreeSansBold12pt7b);
    display.print(mess);
  } while (display.nextPage());
  display.hibernate();
}

void eKairnDisplayTexte(uint16_t rot, char mess[]) {
  display.init(115200, false, 2, false);  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.setRotation(rot);
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(mess, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 3) - tby;
  display.setFullWindow();
  display.firstPage();
  do {
    // Display Message
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(mess);
  } while (display.nextPage());
  display.hibernate();
}
