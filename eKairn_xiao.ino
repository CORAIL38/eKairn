/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#include <bluefruit.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "nrfx_qspi.h"
#include "nrf_clock.h"
#include "nrf_rtc.h"

// Release informations
static const uint32_t EKAIRN_FW_VERSION_HEX = 0x07A0;
#define EKAIRN_FW_VERSION_STR "V07.a.0 27/04/2026"

// ------------- Compilation Options --------------------
// define EK_LOWPOW for low power mode (use in production mode)
// do not define EK_LOWPOW for develeopment mode
#define EK_LOWPOW

// Board Pin assignment
#define MEGA_LED_PIN PIN_A1
#define BUZZER_PIN PIN_A0

// Beacon uses the Manufacturer Specific Data field in the advertising
// packet, which means you must provide a valid Manufacturer ID. Update
// the field below to an appropriate value. For a list of valid IDs see:
// https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
// 0x004C is Apple
// 0x0822 is Adafruit
// 0x0059 is Nordic

// -----------------------------------------------------------------
//    eKairn type
// -----------------------------------------------------------------
enum eKairnType {
  EK_GENERIC = 0,  // Not implemented yet
  EK_EKAZIMUT = 1,
  EK_OTHER = 2,  // Not implementad yet
};

// -----------------------------------------------------------------
//    eKairn security levels
// -----------------------------------------------------------------
enum eKairnSecurityLevel {
  SEC_USR = 0,  // only read
  SEC_ERM = 1,  // can program the device (M,P,T,W,F,A), ERM = Electronic Race Management
  SEC_FAB = 2,  // can change name of the device
  SEC_HWR = 3,  // can chage the Hardware setup
} eKSEC;

// -----------------------------------------------------------------
//    eKairn specific default setup for Orienteering
// -----------------------------------------------------------------
struct eKairnParam {
  char eKairnName[16];
  uint16_t eKairnMarker;
  uint16_t eKairnMajor;
  uint16_t eKairnType;
  int16_t eKairnTx;
  int16_t eKairnPeriod;
  bool eKairnFastReset;
  bool eKairnHWMegaLED;
  bool eKairnHWBuzzer;
  int16_t eKairnHWBat;
  uint16_t eKairnHWManuf;
  int16_t eKairnTimeout;
  uint32_t eKairnHWRKey;
  uint16_t eKairnFABKey;
  uint16_t eKairnERMKey;
  bool eKairnHWDisplay;
  bool eKairnHWSolar;
  uint16_t eKairnDispRot;
  char eKairnMessage[16];
  uint32_t eKairnSpare;  // It seems that configuration must be a multiple of 4 Bytes for QSPI management
};
/******************************************
 *          eKairn Configuration
 *
 */
static struct eKairnParam eKairnDefault = {
  /* 
  .12345678901234567890123456789012 
  */
  "eKairn7        ",  // Name of the device
  123,                // poste = P123
  0,                  // Major = 0
  1,                  // Vikazimut type
  5,                  // Tx Level = -4dBm
  320,                // Period is 200ms
  0,                  // no Fast Reset
  1,                  // MEGA LED
  1,                  // BUZZER
  2,                  // Battery type 550mAH
  0x0059,             // Manufacturer: default Nordic
  0,                  // Timeout (expressed in x5 Minutes)
  0x415D6365,         // HWR Key
  0xCAFE,             // FAB Key
  0x1234,             // ERM Key
  1,                  // ePaper display
  0,                  // Solar
  1,                  // ePaper Display rotation
  "eKairn by fbd38",  // Display Message
  0xA5A5CAFE          // Void
};

char eKairnOFFMessage[] = "Balise de course\nd'orientation\nNe pas deplacer\nCourse terminee\nN'oubliez pas de\npasser a l'accueil";

/*
 *          End on eKairn Configuration
 */

static struct eKairnParam eKairnFactory;
static struct eKairnParam eKairnCurrent;
static bool eKainNeedUpdate = false;
static bool eKainFactoryNeedUpdate = false;
static bool eKSEChasChanged = false;
static bool eKairnStop = false;
static bool eKairnVbatCalibration = false;

void eKairnParamDump(eKairnParam ekp) {
  Serial.println(F("-----------------------------"));
  Serial.println(ekp.eKairnName);
  Serial.println(ekp.eKairnMarker);
  Serial.println(ekp.eKairnMajor);
  Serial.println(ekp.eKairnType);
  Serial.println(ekp.eKairnTx);
  Serial.println(ekp.eKairnPeriod);
  Serial.println(ekp.eKairnFastReset);
  Serial.println(ekp.eKairnHWMegaLED);
  Serial.println(ekp.eKairnHWBuzzer);
  Serial.println(ekp.eKairnHWBat);
  Serial.println(ekp.eKairnHWManuf);
  Serial.println(ekp.eKairnHWRKey);
  Serial.println(ekp.eKairnFABKey);
  Serial.println(ekp.eKairnERMKey);
  Serial.println(ekp.eKairnDispRot);
  Serial.println(ekp.eKairnMessage);
  Serial.println(ekp.eKairnSpare);
  Serial.println(F("-----------------------------"));
}
// Code to avoid mistakes when entering sensitive commands
#define eKairnCodeLow 0xA5F0

#define EKAIRN_MANU_STR "eKairn community"
#define EKAIRN_MODEL_STR "eKairn micro"

// -------- Define QSPI memroy organisation ---------
// Contains FW revision number, use to reload the whole FLASH when FW changes
#define QSPI_mem_BOOT 0
// Default HW Configuration, can be modified in HWR and FAB mode
#define QSPI_mem_SYSTEM 1
// VBat table to convert Voltage in % usage
#define QSPI_mem_VBAT 2
// Current Configuration, can be modified in ERM mode
#define QSPI_mem_CONFIG 3
// For debug purpose (not used)
#define QSPI_mem_DEBUG 8

int8_t nRF_TX[] = { -40, -20, -16, -12, -8, -4, 0, +2, +3, +4, +5, +6, +7, +8 };

uint8_t beaconUuid[16] = {
  0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
  0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0
};

// Battery management
void eKairnFuncHiPow() {
  digitalWrite(MEGA_LED_PIN, HIGH);
}

void eKairnFuncLoPow() {
  digitalWrite(MEGA_LED_PIN, LOW);
}

void eKairnFuncBuzz() {
  // one secound "La"
  for (int16_t i = 0; i < 500; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1);
    digitalWrite(BUZZER_PIN, LOW);
    delay(1);
  }
}
// A valid Beacon packet consists of the following information:
// UUID, Major, Minor, RSSI @ 1M
BLEBeacon beacon(beaconUuid, 0, 0, -54);
BLEDis bledis;    // device information (not mandatory)
BLEUart bleuart;  // uart over ble

// -----------------------------------------------------------------
//  Vikazimut force start and end beacon convention
//
#define EK_FORCE_START (eKairnCurrent.eKairnMarker <= 10)
#define EK_FORCE_END (eKairnCurrent.eKairnMarker >= 256)

// ---- EKAZIMUT Specific data
#define EK_START (1111)
#define EK_END (9999)

uint16_t checkEkMarker(void) {
  if (EK_FORCE_START) return EK_START;
  if (EK_FORCE_END) return EK_END;
  return eKairnCurrent.eKairnMarker;
}

int8_t checkTxPower(void) {
  if (eKairnCurrent.eKairnTx <= 0) return nRF_TX[0];
  if (eKairnCurrent.eKairnTx >= sizeof(nRF_TX)) return nRF_TX[sizeof(nRF_TX)];
  return nRF_TX[eKairnCurrent.eKairnTx];
}

void composeName(void) {
  char *p = &eKairnCurrent.eKairnName[10];
  if (EK_FORCE_START) sprintf(p, "DEP.");
  else if (EK_FORCE_END) sprintf(p, "ARR.");
  else sprintf(p, "P%03d", eKairnCurrent.eKairnMarker);
}

//----------------------------------------------------------------------
//  set Very Low Frequency Tick Timer
//
static int16_t VLFTCount = 0;

void VLFT_Routine(void) {
  VLFTCount--;
  if (VLFTCount <= 0) {
    digitalWrite(LED_GREEN, LOW);
    delay(1000);
    digitalWrite(LED_RED, HIGH);      // OFF
    digitalWrite(LED_BLUE, HIGH);     // OFF
    digitalWrite(LED_GREEN, HIGH);    // OFF
    digitalWrite(MEGA_LED_PIN, LOW);  // OFF
    delay(100);
    // Display if ePaper
    if (eKairnCurrent.eKairnHWDisplay)
      eKairnDisplayTexte(eKairnCurrent.eKairnDispRot, eKairnOFFMessage);
#ifdef EK_LOWPOW
    suspendLoop();
    NRF_POWER->SYSTEMOFF = 1;  // Trigger System OFF --> need RESET to Retstart
#endif
  } else {
    Serial.print(" Adv restart ");
    Serial.println(VLFTCount);
    // Restart for the next time
    startAdv();
  }
}
//-----------------------------------------------------------------------
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(MEGA_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(MEGA_LED_PIN, LOW);  // OFF
  digitalWrite(BUZZER_PIN, LOW);    // OFF

  /* Initialize the serial port */
  Serial.begin(115200);
  Serial.println(F("********* eKairn Started *********"));

  // Assign default values
  eKairnFactory = eKairnDefault;
  eKairnCurrent = eKairnFactory;
  //  eKairnParamDump(eKairnCurrent);
  // Connect the QSPI
  if (QSPI_mem_connect()) {
    Serial.println("[QSPI] Memory connected");
    // Check if the FW has changed (last 4 bits are not taken into account)
    int32_t qmem_frwn;
    QSPI_mem_read(QSPI_mem_BOOT, &qmem_frwn, sizeof(qmem_frwn));
    if ((qmem_frwn >> 4) != (EKAIRN_FW_VERSION_HEX >> 4)) {
      // Reinit all the QSPI memory
      QSPI_mem_erase_all();
      Serial.println(F("[QSPI] all memory erased"));
      digitalWrite(LED_RED, LOW);   // turn the LED on (HIGH is the voltage level)
      delay(2000);                  // wait for a second
      digitalWrite(LED_RED, HIGH);  // turn the LED off by making the voltage LOW
      delay(100);                   // wait for a second
      qmem_frwn = EKAIRN_FW_VERSION_HEX;
      QSPI_mem_write(QSPI_mem_BOOT, &qmem_frwn, sizeof(qmem_frwn));
    }
    // Recover default parameters from the QSPI memory
    if (QSPI_mem_readConfig(QSPI_mem_SYSTEM, &eKairnFactory, &eKairnDefault, sizeof(eKairnParam))) {
      Serial.println("[QSPI] Factory parameters recovered");
    }
    if (QSPI_mem_readConfig(QSPI_mem_CONFIG, &eKairnCurrent, &eKairnFactory, sizeof(eKairnParam))) {
      Serial.println("[QSPI] Current parameters recovered");
    }
    QSPI_mem_disconnect();  // To save power
  } else Serial.println("[QSPI] ERROR cannot connect");

  if (!eKairnCurrent.eKairnFastReset) {
    // First Blue Flash
    digitalWrite(LED_BLUE, LOW);   // turn the LED on
    delay(500);                    // wait for a second
    digitalWrite(LED_BLUE, HIGH);  // turn the LED off
    delay(100);                    // wait for a second
  }
  // Check the Battery
  if (!initVBatTable(eKairnCurrent.eKairnHWBat)) {
    Serial.println("[QSPI] Error recovering VBat parameters, reload default");
    digitalWrite(LED_RED, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(1000);                  // wait for a second
    digitalWrite(LED_RED, HIGH);  // turn the LED off by making the voltage LOW
    delay(100);                   // wait for a second
  }
  // Check Vbat at power up
  if (!eKairnCurrent.eKairnFastReset) {
    // Show level of Battery
    VBatBlink();
    // Blink MEGA_LED if any
    if (eKairnCurrent.eKairnHWMegaLED) {
      digitalWrite(MEGA_LED_PIN, HIGH);  // turn the LED on
      delay(1000);                       // wait for a second
      digitalWrite(MEGA_LED_PIN, LOW);   // turn the LED off
      delay(100);                        // wait for a second
    }
    // Ping Buzzer if any
    if (eKairnCurrent.eKairnHWBuzzer) {
      eKairnFuncBuzz();
    }
    // Display FW version
    Serial.println(EKAIRN_FW_VERSION_STR);
    if (eKairnCurrent.eKairnHWDisplay)
      eKairnDisplayDigit(eKairnCurrent.eKairnDispRot, eKairnCurrent.eKairnMarker, eKairnCurrent.eKairnMessage);
  }
  // Ending Blue Flash
  digitalWrite(LED_BLUE, LOW);   // turn the LED on
  delay(500);                    // wait for a second
  digitalWrite(LED_BLUE, HIGH);  // turn the LED off
  delay(100);                    // wait for a second

  // -------------------------- BLE Configuration ------------------
  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  // off Blue LED for lowest power consumption
  Bluefruit.autoConnLed(false);
  Bluefruit.begin();
  Bluefruit.setTxPower(checkTxPower());  // Check bluefruit.h for supported values
  Serial.println(F(" BLE Started"));

  // composeName();
  // Bluefruit.setName(eKairnCurrent.eKairnName);
  // Bluefruit.setLocalName(eKairn_name);
  // Manufacturer ID is required for Manufacturer Specific Data
  bledis.setManufacturer(EKAIRN_MANU_STR);
  bledis.setModel(EKAIRN_MODEL_STR);
  bledis.setFirmwareRev(EKAIRN_FW_VERSION_STR);
  beacon.setMajorMinor(eKairnCurrent.eKairnMajor, checkEkMarker());  // Vikazimut Marker value
  beacon.setManufacturer(eKairnCurrent.eKairnHWManuf);

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  // Connect Stop routine
  Bluefruit.Advertising.setStopCallback(VLFT_Routine);

  // Configure and Start BLE Uart Service
  bleuart.begin();
  Serial.println(F(" BLE UART Service Started"));
  // Setup the advertising packet
  startAdv();
  Serial.println(F(" BLE Advertising Service Started"));

// Suspend Loop() to save power, since we didn't have any code there
#ifdef EK_LOWPOW
  suspendLoop();
#endif
}

void startAdv(void) {
  // Advertising packet
  // Set the beacon payload using the BLEBeacon class populated
  // earlier in this example
  Bluefruit.Advertising.setBeacon(beacon);
  Bluefruit.Advertising.addTxPower();

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  composeName();
  Bluefruit.setName(eKairnCurrent.eKairnName);
  Bluefruit.ScanResponse.addName();

  // Connect Stop routine
  Bluefruit.Advertising.setStopCallback(VLFT_Routine);

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * Apple Beacon specs
   * - Type: Non connectable, undirected
   * - Fixed interval: 100 ms -> fast = slow = 100 ms
   */
  //Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(eKairnCurrent.eKairnPeriod, eKairnCurrent.eKairnPeriod);  // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);                                                   // number of seconds in fast mode
  if (eKairnCurrent.eKairnTimeout == 0) {
    Bluefruit.Advertising.start(0);  // 0 = Don't stop advertising after n seconds
  } else {
    // Start the verylow frequency timeout to 5 min = 300 sec
    Bluefruit.Advertising.start(300);  // stop and restart advertising every 5 minutes
    if (VLFTCount == 0) VLFTCount = eKairnCurrent.eKairnTimeout;
  }
}

/**
 * Callback invoked when a connection is establisehd
 * @param conn_handle connection where this event happens
 */
void connect_callback(uint16_t conn_handle) {

  // Get the reference to current connection
  BLEConnection *connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);

  // Set BLUE LED
  digitalWrite(LED_RED, HIGH);    // Clear RED LED
  digitalWrite(LED_GREEN, HIGH);  // Clear GREEN LED
  digitalWrite(LED_BLUE, LOW);    // turn the BLUE LED ON

  // Check Vbat when connecting
  VBatBlink();
  // default is no update of the QSPI setup
  eKainNeedUpdate = false;
  eKainFactoryNeedUpdate = false;
  // default is restart advertising when disconnect
  eKairnStop = false;
  // default is security level as low as possible
  eKSEC = SEC_USR;
  eKSEChasChanged = false;
#ifdef EK_LOWPOW
  resumeLoop();
#endif
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;

  Serial.print("Disconnected, reason = 0x");
  Serial.println(reason, HEX);
  if (reason != 0x13) {
    // Error disconnect
    digitalWrite(LED_RED, LOW);  // turn the LED on
    delay(5000);
    digitalWrite(LED_RED, HIGH);  // turn the LED on
    if (eKairnCurrent.eKairnHWBuzzer) {
      eKairnFuncBuzz();
    }
  }
  // Stop Advertising and restart with new Marker value
  Serial.println("EKairn Stop Advertising");
  Bluefruit.Advertising.stop();  // Stop advertising immediatly
  delay(200);                    // To be sure it ends
  // Check if we nee to update the QSPI memeory configuration
  if (eKainNeedUpdate) {
    Serial.println("EKairn QSPI Update");
    if (QSPI_mem_connect()) {
      // write the new configuration into the QSPI memory
      if (QSPI_mem_writeConfig(QSPI_mem_CONFIG, &eKairnCurrent, sizeof(eKairnParam))) {
        digitalWrite(LED_GREEN, LOW);   // turn the LED on (HIGH is the voltage level)
        delay(500);                     // wait for a second
        digitalWrite(LED_GREEN, HIGH);  // turn the LED off by making the voltage LOW
        delay(200);                     // wait for a second
      }
      QSPI_mem_disconnect();
    }
    if (eKainFactoryNeedUpdate) {
      Serial.println("EKairn QSPI Factory Update");
      if (QSPI_mem_connect()) {
        // write the new configuration into the QSPI memory
        if (QSPI_mem_writeConfig(QSPI_mem_SYSTEM, &eKairnFactory, sizeof(eKairnParam))) {
          digitalWrite(LED_GREEN, LOW);   // turn the LED on (HIGH is the voltage level)
          delay(500);                     // wait for a second
          digitalWrite(LED_GREEN, HIGH);  // turn the LED off by making the voltage LOW
          delay(200);                     // wait for a second
        }
      }
      QSPI_mem_disconnect();
    }
  }

  // Assign new values
  Bluefruit.setTxPower(checkTxPower());
  beacon.setMajorMinor(eKairnCurrent.eKairnMajor, checkEkMarker());
  // Restart the eBeacon
  if (!eKairnStop) {
    startAdv();
    Serial.println("EKairn Restart Advertising");
    digitalWrite(LED_BLUE, HIGH);   // turn the BLUE LED OFF
    digitalWrite(LED_GREEN, HIGH);  // turn the GREEN LED OFF
    digitalWrite(LED_RED, HIGH);    // turn the RED LED OFF
  } else {
    digitalWrite(LED_BLUE, HIGH);     // turn the BLUE LED OFF
    digitalWrite(LED_RED, HIGH);      // turn the RED LED OFF
    digitalWrite(MEGA_LED_PIN, LOW);  // turn MEGA_LED OFF
    digitalWrite(LED_GREEN, LOW);
    delay(1000);  // delay seems important to apply settings, before going to System OFF
    digitalWrite(LED_GREEN, HIGH);
    delay(1000);
    NRF_POWER->SYSTEMOFF = 1;  // Trigger System OFF --> need RESET to Retstart
  }
// Then we need no more the main loop, so we goe in low power state
#ifdef EK_LOWPOW
  if (!eKairnVbatCalibration) suspendLoop();
#endif
}

void help() {
  char disp[255] = "";
  sprintf(disp, "-- eKairn list of command --\r\n");
  strcat(disp, "H: Help (this list)\r\n");
  strcat(disp, "I: get Informations\r\n");
  strcat(disp, "S: get Setup\r\n");
  strcat(disp, "V: get battery Voltage\r\n");
  strcat(disp, "Kxxxx: Enter programing");
  //  Then display on both screens
  Serial.println(disp);
  bleuart.write(disp, strlen(disp));
  if (eKSEC >= SEC_ERM) {
    sprintf(disp, "Mddd: set Marker\r\n");
    strcat(disp, "Tdd: set Tx power\r\n");
    strcat(disp, "Pdddd: set Period\r\n");
    strcat(disp, "Wdddd: Set Timeout\r\n");
    strcat(disp, "Z: Factory setup\r\n");
    strcat(disp, "Rx: fast Reset on/off\r\n");
    strcat(disp, "Jxxxx: set maJor\r\n");
    strcat(disp, "Q: Quite device\r\n");
    if (eKairnCurrent.eKairnHWMegaLED)
      strcat(disp, "L, l: manage mega LED\r\n");
    if (eKairnCurrent.eKairnHWBuzzer)
      strcat(disp, "B: Buzzer\r\n");
    if (eKairnCurrent.eKairnHWDisplay) {
      strcat(disp, "Gd: rotate display\r\n");
      strcat(disp, "Acccccccccccccccc: display message\r\n");
    }
    strcat(disp, "$xxxx: Enter FAB mode");
    Serial.println(disp);
    bleuart.write(disp, strlen(disp));
    if (eKSEC >= SEC_FAB) {
      sprintf(disp, "Ncccccccccc: Set Mame\r\n");
      strcat(disp, "Exxxx: set EMR key");
      Serial.println(disp);
      bleuart.write(disp, strlen(disp));
      if (eKSEC >= SEC_HWR) {
        sprintf(disp, "!xxxx: set manufacturer\r\n");
        strcat(disp, "Oxxxx: set hw Options\r\n");
        strcat(disp, "Cxxxx: VBat Calibrate\r\n");
        strcat(disp, "Dxxxx: Dump Flash mem.\r\n");
        strcat(disp, "Fxxxx: set FAB key\r\n");
        strcat(disp, ".A5F0: clear Flash");
        Serial.println(disp);
        bleuart.write(disp, strlen(disp));
      }
    }
  }
}

// Function to get a 16 bit unsigned integer from the BLE UART
uint16_t readBLE16hex() {
  char st[8] = "";
  bleuart.read(st, 4);
  uint16_t val;
  sscanf(st, "%4x", &val);
  Serial.print(" (");
  Serial.print(val, HEX);
  Serial.println(")");
  return (val);
}
// Function to get a 32 bit unsigned integer from the BLE UART
uint32_t readBLE32hex() {
  char st[16] = "";
  bleuart.read(st, 8);
  uint32_t val;
  sscanf(st, "%8x", &val);
  Serial.print(" (");
  Serial.print(val, HEX);
  Serial.println(")");
  return (val);
}
// Function to get a 16 bit signed integer from the BLE UART
int16_t readBLE16dec() {
  char st[8] = "";
  bleuart.read(st, 8);
  int16_t val;
  sscanf(st, "%d", &val);
  Serial.print(" (");
  Serial.print(val);
  Serial.println(")");
  return (val);
}

void loop() {
  // loop is already suspended, CPU will not run loop() at all
  // Forward from BLEUART to HW Serial
  while (bleuart.available()) {
    uint8_t ch;
    bool valCom = false;
    ch = (uint8_t)bleuart.read();
    Serial.print("Rx BLE command = [");
    Serial.print((char)ch);
    Serial.println("]");

    // Parser the list of commands
    // The display help message according with security level
    if ((ch == 'H') || (ch == 'h') || (ch == '?')) {
      help();
      valCom = true;
    }
    if ((ch == 'I') || (ch == 'i')) {
      char disp[255] = "";
      sprintf(disp, EKAIRN_MODEL_STR);
      strcat(disp, "\n\r");
      strcat(disp, EKAIRN_FW_VERSION_STR);
      Serial.println(disp);
      bleuart.write(disp, strlen(disp));
      valCom = true;
    }
    if ((ch == 'S') || (ch == 's')) {
      // Display status
      char st[16] = "";
      sprintf(st, "Marker = %3d", eKairnCurrent.eKairnMarker);
      if (EK_FORCE_START) sprintf(st, "Marker = START");
      if (EK_FORCE_END) sprintf(st, "Marker = END");
      // Write current Tx  Power
      char st2[16] = "";
      sprintf(st2, "Tx = %2d dBm", nRF_TX[eKairnCurrent.eKairnTx]);
      // Write current Period
      char st3[20] = "";
      sprintf(st3, "Period = %5.0f ms", ((float)eKairnCurrent.eKairnPeriod) * 0.62);
      // Write current Timeout
      char st4[40] = "";
      if (eKairnCurrent.eKairnTimeout != 0) {
        int16_t tmps = eKairnCurrent.eKairnTimeout;
        int16_t days = tmps / 288;
        int16_t hours = (tmps - days * 288) / 12;
        int16_t mins = (tmps - days * 288 - hours * 12) * 5;
        sprintf(st4, "Timeout = %2d d, %2d h, %2d m", days, hours, mins);
      } else sprintf(st4, "Timeout = 0");
      char disp[255] = "";
      sprintf(disp, st);
      strcat(disp, "\n\r");
      strcat(disp, st2);
      strcat(disp, "\n\r");
      strcat(disp, st3);
      strcat(disp, "\n\r");
      strcat(disp, st4);
      Serial.println(disp);
      bleuart.write(disp, strlen(disp));
      if (eKSEC >= SEC_ERM) {
        char str[20] = "";
        sprintf(disp, "\n\r%s", eKairnCurrent.eKairnName);
        strcat(disp, "\n\r");
        sprintf(str, "Major = %4x\n\r", eKairnCurrent.eKairnMajor);
        strcat(disp, str);
        if (eKairnCurrent.eKairnFastReset) strcat(disp, "Fast Reset\n\r");
        sprintf(str, "ERM Key = %4x", eKairnCurrent.eKairnERMKey);
        strcat(disp, str);
        Serial.println(disp);
        bleuart.write(disp, strlen(disp));
        if (eKSEC >= SEC_FAB) {
          char str[20] = "";
          sprintf(disp, "Manuf. UID = %4x\n\r", eKairnCurrent.eKairnHWManuf);
          sprintf(str, "HWR = %d, %d, %d\n\r", eKairnCurrent.eKairnHWBat, eKairnCurrent.eKairnHWMegaLED, eKairnCurrent.eKairnHWBuzzer);
          strcat(disp, str);
          sprintf(str, "FAB Key = %4x", eKairnCurrent.eKairnFABKey);
          strcat(disp, str);
          Serial.println(disp);
          bleuart.write(disp, strlen(disp));
        }
      }
      valCom = true;
    }
    if ((ch == 'V') || (ch == 'v')) {
      // Perform a VBat acquisition
      float VBat = ReadVbat();
      Serial.print("VBat = ");
      char cvbat[64];
      sprintf(cvbat, "%5.3f V = %3d %% (%6.0f)", VBat, ConvertVBatInPercent(VBat), BatCapacityRemaining(VBat));
      Serial.println(cvbat);
      bleuart.write(cvbat, strlen(cvbat));
      valCom = true;
    }
    // Then check commands allowed in ERM mode
    if (eKSEC >= SEC_ERM) {
      if ((ch == 'M') || (ch == 'm')) {
        char st[8] = "";
        uint16_t nval = readBLE16dec();
        if ((nval < 0) || (nval > 999)) {
          const char err[] = "ERROR Invalid Value, must be [0..999]";
          Serial.println(err);
          bleuart.write(err, sizeof(err));
        } else {
          eKairnCurrent.eKairnMarker = nval;
          eKainNeedUpdate = true;
          if (eKSEC >= SEC_FAB) {
            eKairnFactory.eKairnMarker = nval;
            eKainFactoryNeedUpdate = true;
          }
          // Update the display if any
          if (eKairnCurrent.eKairnHWDisplay)
            eKairnDisplayDigit(eKairnCurrent.eKairnDispRot, eKairnCurrent.eKairnMarker, eKairnCurrent.eKairnMessage);
        }
        valCom = true;
      }
      // Set TX level
      if ((ch == 'T') || (ch == 't')) {
        int16_t nval = readBLE16dec();
        if ((nval < 0) || (nval > 13)) {
          const char err[] = "ERROR Invalid Value, must be [0..13]";
          Serial.println(err);
          bleuart.write(err, sizeof(err));
        } else {
          eKairnCurrent.eKairnTx = nval;
          eKainNeedUpdate = true;
          if (eKSEC >= SEC_FAB) {
            eKairnFactory.eKairnTx = nval;
            eKainFactoryNeedUpdate = true;
          }
        }
        valCom = true;
      }
      if ((ch == 'P') || (ch == 'p')) {
        int16_t nval = readBLE16dec();
        if ((nval < 100) || (nval > 1600)) {
          const char err[] = "ERROR Invalid Value, must be [100..1600]";
          Serial.println(err);
          bleuart.write(err, sizeof(err));
        } else {
          eKairnCurrent.eKairnPeriod = nval;
          eKainNeedUpdate = true;
          if (eKSEC >= SEC_FAB) {
            eKairnFactory.eKairnPeriod = nval;
            eKainFactoryNeedUpdate = true;
          }
        }
        valCom = true;
      }
      if ((ch == 'W') || (ch == 'w')) {
        int16_t nval = readBLE16dec();
        if ((nval < 0) || (nval > 8928)) {
          const char err[] = "ERROR Invalid Value, must be [0..8928] (31 jours max!)";
          Serial.println(err);
          bleuart.write(err, sizeof(err));
        } else {
          eKairnCurrent.eKairnTimeout = nval;
          eKainNeedUpdate = true;
          if (eKSEC >= SEC_FAB) {
            eKairnFactory.eKairnTimeout = nval;
            eKainFactoryNeedUpdate = true;
          }
        }
        valCom = true;
      }
      if ((ch == 'Z') || (ch == 'z')) {
        eKairnCurrent = eKairnFactory;
        eKainNeedUpdate = true;
        valCom = true;
      }
      // R: fast Reset
      if ((ch == 'R') || (ch == 'r')) {
        uint8_t ch;
        ch = (uint8_t)bleuart.read();
        if (ch == '1')
          eKairnCurrent.eKairnFastReset = true;
        else eKairnCurrent.eKairnFastReset = false;
        eKainNeedUpdate = true;
        if (eKSEC >= SEC_FAB) {
          eKairnFactory.eKairnFastReset = eKairnCurrent.eKairnFastReset;
          eKainFactoryNeedUpdate = true;
        }
        valCom = true;
      }
      // Jxyzt: set maJor value
      if ((ch == 'J') || (ch == 'j')) {
        int16_t val = readBLE16hex();
        eKairnCurrent.eKairnMajor = val;
        eKainNeedUpdate = true;
        if (eKSEC >= SEC_FAB) {
          eKairnFactory.eKairnMajor = val;
          eKainFactoryNeedUpdate = true;
        }
        valCom = true;
      }
      // Stop the device, will rerquire a reset or power up to restart
      if ((ch == 'Q') || (ch == 'q')) {
        Serial.println("[Device Stop] use RESET Magnet or PWR ON to restart");
        eKairnStop = true;
        valCom = true;
      }
      if (eKairnCurrent.eKairnHWMegaLED) {
        if (ch == 'L') {
          digitalWrite(MEGA_LED_PIN, HIGH);  // turn the Mega LED ON
          valCom = true;
        }
        if (ch == 'l') {
          digitalWrite(MEGA_LED_PIN, LOW);  // turn the Mega LED OFF
          valCom = true;
        }
      }
      if (eKairnCurrent.eKairnHWBuzzer) {
        if ((ch == 'B') || (ch == 'b')) {
          eKairnFuncBuzz();
          valCom = true;
        }
      }
      if (eKairnCurrent.eKairnHWDisplay) {
        if ((ch == 'G') || (ch == 'g')) {
          int16_t nval = readBLE16dec();
          if ((nval < 0) || (nval > 3)) {
            const char err[] = "ERROR Invalid Value, must be [0..3]";
            Serial.println(err);
            bleuart.write(err, sizeof(err));
          } else {
            eKairnCurrent.eKairnDispRot = nval;
            eKainNeedUpdate = true;
            if (eKSEC >= SEC_FAB) {
              eKairnFactory.eKairnDispRot = nval;
              eKainFactoryNeedUpdate = true;
            }
            valCom = true;
            // Update display immediately
            eKairnDisplayDigit(eKairnCurrent.eKairnDispRot, eKairnCurrent.eKairnMarker, eKairnCurrent.eKairnMessage);
          }
        }
        if ((ch == 'A') || (ch == 'a')) {
          eKairnFuncBuzz();
          valCom = true;
          eKairnDisplayDigit(eKairnCurrent.eKairnDispRot, eKairnCurrent.eKairnMarker, eKairnCurrent.eKairnMessage);
        }
      }
      if (eKSEC >= SEC_FAB) {
        // Nxxxxxxxx change name of the device
        if ((ch == 'N') || (ch == 'n')) {
          char new_name[10];
          bool err = false;
          int i = 0;
          while ((i < 8) && (!err)) {
            err = !bleuart.available();
            if (!err) new_name[i++] = (uint8_t)bleuart.read();
          }
          if (!err) {
            for (i = 0; i < 8; i++) eKairnCurrent.eKairnName[i] = new_name[i];
            Serial.print("Name changed to [");
            Serial.print(eKairnCurrent.eKairnName);
            Serial.println("]");
          }
          for (i = 0; i < 8; i++) eKairnFactory.eKairnName[i] = eKairnCurrent.eKairnName[i];
          eKainFactoryNeedUpdate = true;
          eKainNeedUpdate = true;
          valCom = true;
        }
        // Exxx set EMR key
        if ((ch == 'E') || (ch == 'e')) {
          uint16_t val = readBLE16hex();
          eKairnCurrent.eKairnERMKey = val;
          eKairnFactory.eKairnERMKey = val;
          eKainFactoryNeedUpdate = true;
          eKainNeedUpdate = true;
          valCom = true;
        }
        if (eKSEC >= SEC_HWR) {
          // !xyzt: change manufacturer UUID code
          if (ch == '!') {
            uint16_t val = readBLE16hex();
            eKairnCurrent.eKairnHWManuf = val;
            eKairnFactory.eKairnHWManuf = val;
            eKainFactoryNeedUpdate = true;
            eKainNeedUpdate = true;
            valCom = true;
          }
          // Oxxxx: define HW options (BAT, LED, BUZ..)
          if ((ch == 'O') || (ch == 'o')) {
            uint16_t val = readBLE16hex();
            uint16_t iv;
            // Assign VBat table
            iv = ((val & 0x00F0) >> 4) & 0x000F;
            if (iv >= MaxNumVBatTable()) {
              eKairnCurrent.eKairnHWBat = 0;
              const char err[] = "ERROR Invalid Battery type";
              Serial.println(err);
              bleuart.write(err, sizeof(err));
            } else eKairnCurrent.eKairnHWBat = iv;
            // Assign BUZZER
            iv = val & 0x0001;
            eKairnCurrent.eKairnHWBuzzer = iv;
            // Assign MEGA_LED
            iv = (val & 0x0002) >> 1;
            eKairnCurrent.eKairnHWMegaLED = iv;
            // Assign ePaper Display
            iv = (val & 0x0004) >> 2;
            eKairnCurrent.eKairnHWDisplay = iv;
            // Assign Solar
            iv = (val & 0x0008) >> 3;
            eKairnCurrent.eKairnHWSolar = iv;
            // Assign Factory parameters
            eKairnFactory.eKairnHWBat = eKairnCurrent.eKairnHWBat;
            eKairnFactory.eKairnHWMegaLED = eKairnCurrent.eKairnHWMegaLED;
            eKairnFactory.eKairnHWBuzzer = eKairnCurrent.eKairnHWBuzzer;
            eKainFactoryNeedUpdate = true;
            eKainNeedUpdate = true;
            valCom = true;
          }
          // Calibrate VBat - will take a day or so
          if ((ch == 'C') || (ch == 'c')) {
            if (readBLE16hex() == eKairnCodeLow) {
              Serial.println("[ERR] you should remove the USB connection");
              eKairnStop = true;  // needed in case of deconnection
              eKairnVbatCalibration = true;
              VBatCalibration(eKairnFuncHiPow, eKairnFuncLoPow);  // 10mV step --> about 60 points
              eKairnVbatCalibration = false;
            }
            valCom = true;
          }
          // Dump the QSPI Memory
          if ((ch == 'D') || (ch == 'd')) {
            // address is BBBB BB0L LLLL LLLL
            // where B is bit for block number and L for line number
            uint32_t adl = readBLE16hex();
            uint16_t b, l;
            b = adl >> 10;
            l = adl & 0x1FF;
            l = max(l, 1);  // In case it is 0
            char st[128];
            sprintf(st, "[QSPI] Memory dump : block %d number of lines %d", b, l);
            Serial.println(st);
            QSPI_mem_dump(b, l);
            valCom = true;
          }
          // Eraze the QSPI Memory
          if (ch == '.') {
            if (readBLE16hex() == eKairnCodeLow) {
              QSPI_mem_erase_all();
              Serial.println(F("[QSPI] all memory erased"));
              valCom = true;
            }
          }
        }  // SEC_HWR
      }    // SEC_FAB
    }      // SEC_ERM


    // ----- Security level management
    // Check change in security level to ERM
    if ((ch == 'K') || (ch == 'k')) {
      if (valCom = (readBLE16hex() == eKairnCurrent.eKairnERMKey)) {
        eKSEC = SEC_ERM;
        eKSEChasChanged = true;
      }
    }
    // Check change in security level to FAB
    if (ch == '$') {
      if (valCom = (readBLE16hex() == eKairnCurrent.eKairnFABKey)) {
        eKSEC = SEC_FAB;
        eKSEChasChanged = true;
      } else delay(1000);  // To prevent Brute Force
    }
    // Check change in security level to HWR
    if (ch == '@') {
      if (valCom = (readBLE32hex() == eKairnCurrent.eKairnHWRKey)) {
        eKSEC = SEC_HWR;
        eKSEChasChanged = true;
      } else {
        delay(10000);  // To prevent brute force
      }
    }
    // In case of error command
    if (!valCom) {
      const char err[] = "ERROR Invalid Key, Command or Mode";
      uint8_t ch;
      Serial.println(err);
      bleuart.write(err, sizeof(err));
      while (bleuart.available()) {  // Flush imput buffer
        ch = (uint8_t)bleuart.read();
      }
    }
    // Update multicolor led with Security level
    if (eKSEChasChanged) {
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, HIGH);
      switch (eKSEC) {
        case SEC_USR:                   // BLUE
          digitalWrite(LED_BLUE, LOW);  // turn the BLUE LED ON
          break;
        case SEC_ERM:                    // GREEN
          digitalWrite(LED_GREEN, LOW);  // turn the BLUE LED ON
          break;
        case SEC_FAB:                   // PURPLE
          digitalWrite(LED_BLUE, LOW);  // turn the BLUE LED ON
          digitalWrite(LED_RED, LOW);   // turn the RED LED ON
          break;
        case SEC_HWR:                  // RED
          digitalWrite(LED_RED, LOW);  // turn the RED LED ON
      }
      eKSEChasChanged = false;
    }
  }
}
