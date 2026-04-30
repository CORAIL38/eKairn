//
// XIAO VBAT Measurement and Management
//
//  Author: fbd38
//  Release: 2.2.0
//  Date:   06/06/2024
//  Update: 27/04/2026
//
// Batery voltage is expressed in Volt (f format) and mV (uint16_t)
//  Discharge Battery must be done at about 1/10 of C which gives
//  for a 150mAh : 15 mA --> 220 Ohm connected to V3.3
//  for a 550mAh : 50 mA --> 68 Ohm connected to V3.3
//  for a 50mAh (solar) : 5 mA --> 680 Ohm connected to V3.3
//  Discharge time is about 10 Hours (one night)
//
//  for information on batteries consult: hhtps://batteryuniversity.com
//
static const int16_t VbatMax100 = 4100;  // Maximum voltage, in mV, to declare 100% in mV
static const int16_t VbatMin000 = 3500;  // Minimum voltage for 0% (= 3.3V + 0.2V)
struct VbatPoint {
  int16_t v;  // voltage in mV
  int32_t t;  // time or Percent
};
int16_t voltStep = 20;  // 20mV --> 31 points maximum
#define MAX_VBAT_TABLE 60
static VbatPoint VbatPercentTable[MAX_VBAT_TABLE];  // Table to store the Vbat and Percentage values
int16_t indexMax = 0;
int32_t calibrationDuration = 0;

// --------- VBAT Tables ----------
int16_t MaxNumVBatTable(void) {
  return (4);
}

// Generic table for a basic LiPoly battery with 3.7V operationg point (based on 2000mA/H)
static VbatPoint VbatMiddleTableGeneric[6] = {
  { .v = 4100, .t = 100 },
  { .v = 4000, .t = 90 },
  { .v = 3750, .t = 50 },
  { .v = 3640, .t = 25 },
  { .v = 3500, .t = 0 },  // last point must have t=0
  { .v = 0, .t = 4000 },  // Last point is duration in minutes of calibration which give an idea of the battery capacity
};
// Setup for EEM3 LP401730 150mAH Battery
static VbatPoint VbatMiddleTable150mAH[6] = {
  { .v = 4100, .t = 100 },
  { .v = 3680, .t = 50 },
  { .v = 3640, .t = 40 },
  { .v = 3560, .t = 12 },
  { .v = 3500, .t = 0 },  // last point must have t=0
  { .v = 0, .t = 552 },   // Last point is duration in minutes of calibration which give an idea of the battery capacity
};
// Setup for EEM3 LP401730 550mAH Battery
static VbatPoint VbatMiddleTable550mAH[8] = {
  { .v = 4100, .t = 100 },
  { .v = 4000, .t = 90 },
  { .v = 3940, .t = 75 },
  { .v = 3850, .t = 60 },
  { .v = 3780, .t = 40 },
  { .v = 3540, .t = 5 },
  { .v = 3500, .t = 0 },  // last point must have t=0
  { .v = 0, .t = 1970 },  // Last point is duration in minutes of calibration which give an idea of the battery capacity
};

// Setup for solar supercap 50F, 50mAH equivalent Battery
static VbatPoint VbatMiddleTable50mAH[5] = {
  { .v = 4000, .t = 100 },
  { .v = 3950, .t = 90 },
  { .v = 3400, .t = 10 },
  { .v = 3350, .t = 0 },  // last point must have t=0
  { .v = 0, .t = 180 },  // Last point is duration in minutes of calibration which give an idea of the battery capacity
};

// Calibration could depends of the board. It can be performed by measuring the battery voltage with a multimeter
static const float VBatAdj = 1.025;  // adjustment = 2.5% Uncertainty -> in specifications

// Set the GPIO PIN and ADC configuration for VBat measurements
void SetVBatMeasurement(void) {
  pinMode(PIN_VBAT, INPUT);          //Battery Voltage monitoring pin
  pinMode(VBAT_ENABLE, OUTPUT);      //Enable Battery Voltage monitoring pin
  digitalWrite(VBAT_ENABLE, LOW);    //Enable
  analogReference(AR_INTERNAL_2_4);  //Vref=2.4V
  analogReadResolution(12);          //12bits
}

// Perform a VBat voltage Read
float ReadVbat(void) {
  // Read VBAT voltage
  pinMode(VBAT_ENABLE, OUTPUT);    // Enable Battery Voltage monitoring pin
  digitalWrite(VBAT_ENABLE, LOW);  // Enable
  delay(200);                      // To be sure the pin voltage is stable
  int VBatMin = 4096;
  int VBatMax = 0;
  int VBat = 0;
  for (int i = 0; i < 10; i++) {
    int v = analogRead(PIN_VBAT);
    VBatMin = min(VBatMin, v);
    VBatMax = max(VBatMax, v);
    VBat += v;
    delay(50);
  }
  VBat -= VBatMin;  // Remove min ADC value
  VBat -= VBatMax;  // Remove max ADC value
  VBat /= 8;
  pinMode(VBAT_ENABLE, INPUT);  //Disable Battery Voltage monitoring pin
  float Vbattery = ((510e3 + 1000e3) / 510e3) * 2.4 * VBat / 4096;
  return (Vbattery * VBatAdj);
}
// Convert VBat in %
//  use a table that can be optained eitheir with reference points provided by the battery manufacturer
//  or throught the calibration rouitine bellow
int16_t ConvertVBatInPercent(float v) {
  return (grabVbatTable(v));
}

// Get remaining capacity
float BatCapacityRemaining(float v) {
  return (((float)calibrationDuration * ConvertVBatInPercent(v)) / 100.0);
}

// Convert VBat % in RED LED flashes
void VBatBlink() {
  // Enable the VBat measurement
  SetVBatMeasurement();
  float VBat = ReadVbat();
  int16_t iPercent = ConvertVBatInPercent(VBat);
  int ib = 10 - (iPercent / 10);
  float CapBat = BatCapacityRemaining(VBat);
  Serial.print("VBat = ");
  Serial.print(VBat, 3);
  Serial.print(" V  = ");
  Serial.print(iPercent);
  Serial.print(" % (");
  Serial.print(calibrationDuration);
  Serial.print(" min) ");
  Serial.println(CapBat, 0);
  // performs RED flashes according with battery remaining content
  if (ib > 0) {
    for (int ii = 0; ii < ib; ii++) {
      digitalWrite(LED_RED, LOW);   // turn the LED on
      delay(250);                   // wait for 1/4 a second
      digitalWrite(LED_RED, HIGH);  // turn the LED off
      delay(250);                   // wait for 1/4 a second
    }
  }
}

// Initialise the VBat table eitheir from Middle point or calibration
bool initVBatTable(int DefaultBatTable) {
  bool success = true;
  memset(VbatPercentTable, 0, MAX_VBAT_TABLE * sizeof(VbatPoint));  // Clear all the table
  switch (DefaultBatTable) {
    case 1:
      memcpy(VbatPercentTable, VbatMiddleTable150mAH, sizeof(VbatMiddleTable150mAH));  // Copy with default value
      break;
    case 2:
      memcpy(VbatPercentTable, VbatMiddleTable550mAH, sizeof(VbatMiddleTable550mAH));  // Copy with default value
      break;
    case 3:
      memcpy(VbatPercentTable, VbatMiddleTable50mAH, sizeof(VbatMiddleTable50mAH));  // Copy with default value
      break;
    default:
      memcpy(VbatPercentTable, VbatMiddleTableGeneric, sizeof(VbatMiddleTableGeneric));  // Copy with default value
  }
  success = QSPI_mem_connect();
  if (success) {
    success = QSPI_mem_readConfig(QSPI_mem_VBAT, &VbatPercentTable, &VbatPercentTable, sizeof(VbatPercentTable));  // necessary as size of VbatMiddleTable is < VbatPercentTable
    // for debug purpose save the table
    // char str[] = "--- VBat Mirror  ---";
    // QSPI_mem_writeConfig(QSPI_mem_DEBUG, &str, sizeof(str));
  }
  QSPI_mem_disconnect();
  // Compute IndexMax
  for (indexMax = 0; VbatPercentTable[indexMax].t > 0; indexMax++)
    ;
  indexMax = (indexMax >= (MAX_VBAT_TABLE - 1) ? (MAX_VBAT_TABLE - 1) : indexMax + 1);
  calibrationDuration = VbatPercentTable[indexMax].t;
  return (success);
}

//
// Battery calibration (see documentation, to perform for manufacturing modules)
//
//  Special sequence that can take few hours
//  requires two functions:
//     HiPow (high power giving the C/10 current)
//     LoPow (stopping the C/10 current)
//
void VBatCalibration(void funcHiPow(), void funcLoPow()) {
  char str[64];  // reserve enought room
  uint16_t index = 0;
  uint32_t currMillis;
  sprintf(str, "* Battery calibration Started * \r");
  bleuart.write(str, strlen(str));
  // Test if voltage is hight enought
  int16_t currVolt;
  currVolt = round(ReadVbat() * 1000);
  sprintf(str, "[VBat] VbatMax = %d, VbatMin = %d, voltStep = %d, VBatAdj = %f", VbatMax100, VbatMin000, voltStep, VBatAdj);
  bleuart.write(str, strlen(str));
  if (currVolt < (VbatMax100 + voltStep)) {
    sprintf(str, "[ERR] VBat voltage is too low, please charge it\r");
    bleuart.write(str, strlen(str));
    sprintf(str, "      VBat = %5.3f  V, it should be > %5.3f V\r", currVolt, VbatMax100 + voltStep);
    bleuart.write(str, strlen(str));
  } else {
    // Go for High Power
    funcHiPow();
    uint32_t StartMillis;
    int16_t currThreshold = VbatMax100;
    do {
      // [TODO] optimize timing so that the Batery resistor not taken into account
      // Read battery voltage
      currVolt = round(ReadVbat() * 1000);
      currMillis = millis();  // Get time
      if (currVolt < currThreshold) {
        if (index == 0) {
          // Initial Point
          VbatPercentTable[index].v = VbatMax100;
          VbatPercentTable[index].t = 0;
          StartMillis = currMillis;
        } else {
          VbatPercentTable[index].v = currVolt;
          VbatPercentTable[index].t = (currMillis - StartMillis) / 1000;  // Second resolution is enougth
        }
        // sprintf(str, "[%02d], Time : %d sec, VBat = %5.3f  V\r", index, VbatPercentTable[index].t, VbatPercentTable[index].v);
        // bleuart.write(str, strlen(str));
        currThreshold -= voltStep;
        index++;
      }
      // [TODO] then back to HiPow
      // Start consuming about 1/C
      //  funcHiPow();
      // Show some LED activity
      digitalWrite(LED_RED, LOW);   // turn the LED on
      delay(100);                   // duration
      digitalWrite(LED_RED, HIGH);  // turn the LED off
      delay(4000);
      // Reduce consumption so that the Resistance of the battery is not taken into account
      //  funcLoPow();
    } while ((currVolt > VbatMin000) && (index < (MAX_VBAT_TABLE - 1)));
    // Final Point
    indexMax = index;
    currMillis = millis();  // Get time
    int32_t secondMax = (currMillis - StartMillis) / 1000;
    VbatPercentTable[indexMax].v = VbatMin000;
    VbatPercentTable[indexMax].t = secondMax;
    // Back to low power
    funcLoPow();
    // End of calibration Phase
    // sprintf(str, "** Calibration end ** \r");
    // bleuart.write(str, strlen(str));
    // Last point of the table is 0 and SecondMax [TODO] for absolute capacitance computation
    calibrationDuration = secondMax / 60;  // a bit redondant
    VbatPercentTable[indexMax + 1].v = 0;
    VbatPercentTable[indexMax + 1].t = calibrationDuration;  // Total duration of the calibration Phase in minute
    // Compute Percentage
    for (int ii = indexMax; ii >= 0; ii--) {
      float p = 100.0 * (1.0 - ((float)VbatPercentTable[ii].t / (float)secondMax));
      VbatPercentTable[ii].t = max(min(round(p), 100), 0);
    }
    // Store the data in the QSPI memory
    if (QSPI_mem_connect()) {
      if (!QSPI_mem_writeConfig(QSPI_mem_VBAT, &VbatPercentTable, sizeof(VbatPercentTable))) {
        // sprintf(str, "[ERR] VBat voltage calibration QSPI write\r");  // Comment: Lot of chance that this message get lost
        // bleuart.write(str, strlen(str));
      }
      // for debug purpose save the table
      // QSPI_mem_writeConfig(QSPI_mem_DEBUG, &VbatPercentTable, sizeof(VbatPercentTable));
      QSPI_mem_disconnect();
    }

    // Dump Percentage table (same comment)
    // for (int ii = 0; ii <= indexMax; ii++) {
    //   // [TODO] check that V and T are decreasing
    //   sprintf(str, "[%02d], VBat = %5.3f  V  --> Percent = %3d\r", ii, VbatPercentTable[ii].v, VbatPercentTable[ii].t);
    //   bleuart.write(str, strlen(str));
    // }
  }
}

// Linear interpolation to compute Percentage
int16_t grabVbatTable(float Volt) {
  // char str[128];
  int16_t V = round(Volt * 1000.0);
  // sprintf(str, "GrabVBatTable: %f, %d, (%d, %d)", Volt, V, VbatMin000, VbatMax100);
  // Serial.println(str);
  if (V >= VbatMax100) {
    return (100);
  }
  if (V <= VbatMin000) {
    return (0);
  }
  // Found the voltage range
  int ii;
  // sprintf(str, "Value zero: [%02d], %d -> %d", 0, VbatPercentTable[0].v, VbatPercentTable[0].t);
  // Serial.println(str);
  for (ii = 0; ((VbatPercentTable[ii].v > V) && (ii < indexMax)); ii++) {
    /* nothing */
    // sprintf(str, "Value: [%02d], %d -> %d", ii, VbatPercentTable[ii].v, VbatPercentTable[ii].t);
    // Serial.println(str);
  }
  if (ii >= indexMax) return (0);  // in case
  if (ii <= 0) return (100);       // in case
  // Linear interpolation between the two points
  float dv = VbatPercentTable[ii - 1].v - VbatPercentTable[ii].v;
  float dp = (float)(VbatPercentTable[ii - 1].t - VbatPercentTable[ii].t);
  // sprintf(str, "Interpolation: %d, %f, %f", ii, dv, dp);
  // Serial.println(str);
  // [TODO] check that dv and dp are positives
  return (round(((float)VbatPercentTable[ii].t) + dp * (V - VbatPercentTable[ii].v) / dv));
}