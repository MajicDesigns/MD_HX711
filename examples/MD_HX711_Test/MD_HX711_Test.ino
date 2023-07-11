// Test code for library functions
//
// One additional switch are required to initiate zero tare and calibration using
// single press and double press respectively.
// 
// Calibration of the HX711 DAC values will be to the weight specified in the constant 
// CALIBRATE_WEIGHT which will also depend on the load cell rating capacity.
// 
// Dependencies
// MD_UISwitch from https:://github.com/MajicDesigns/MD_UISwitch or the Arduino IDE Library Manager
//

#include <MD_HX711.h>
#include <MD_UISwitch.h>

#define ENABLE_CH_B    0    // set 1 to enable channel B
#define ENABLE_IRQ     0    // set 1 to enable interrupt mode
#define ENABLE_LORES_A 0    // set 1 to enable lower gain on channel A

const float CALIBRATE_WEIGHT = 1000.0;    // calibration weight in grams

// Define pin connections to HX711 module
const uint8_t PIN_DAT = 2;
const uint8_t PIN_CLK = 4;

MD_HX711 scale(PIN_CLK, PIN_DAT);

// Define pin connctions for switches
const uint8_t PIN_SCALE = 5;

MD_UISwitch_Digital swScale(PIN_SCALE);

void setup(void)
{
  Serial.begin(57600);
  Serial.println("[MD_HX711 Test]");

  swScale.begin();    // switch initilization

  scale.begin();      // scale initialization
#if ENABLE_CH_B
  scale.enableChannelB();
#endif
#if ENABLE_LORES_A
  scale.setGainA(MD_HX711::GAIN_64);
#endif
#if ENABLE_IRQ
  scale.enableInterruptMode();
#endif
}

void loop(void)
{
  static uint32_t lastRead = 0;

#if ENABLE_IRQ
  if (lastRead != scale.getReadCount())
#else
  if (scale.isReady())
#endif
  {
    // display weight
    MD_HX711::channel_t ch = scale.read();

    lastRead = scale.getReadCount();

    Serial.print(lastRead);
    Serial.print(ch == MD_HX711::CH_A ? "\tA" : "\tB");
    Serial.print(" r ");
    Serial.print(scale.getRaw(ch));
    Serial.print("\tt ");
    Serial.print(scale.getTared(ch));
    Serial.print("\tc ");
    Serial.println(scale.getCalibrated(ch), 2);
  }

  // process switch
  switch (swScale.read())
  {
    case MD_UISwitch::KEY_PRESS:
    {
      Serial.println("-> Zero Tare");
      scale.autoZeroTare();
    }
    break;

    case MD_UISwitch::KEY_DPRESS:
    {
      Serial.println("-> Calibrate ");
      Serial.print(CALIBRATE_WEIGHT);
      scale.setCalibration(scale.getRaw(MD_HX711::CH_A), CALIBRATE_WEIGHT, MD_HX711::CH_A);
    }
    break;

    default:
      break;
  }
}