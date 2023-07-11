// Implementation of simple Weigh Scale using MD_HX711 library
//
// Simple weigh scale that can be tared and calibrated using a
// single switch. Weight from Channel A (default) is 
// continuously displayed on a 2 line I2C LCD module display. 
// 
// Single press of the switch sets the tare, double press sets 
// the calibration.
// 
// Calibration of the HX711 DAC values will be to the weight 
// specified in the constant CALIBRATE_WEIGHT which will also 
// depend on the load cell rating capacity. Tare and calibration 
// settings are stored in EEPROM and loaded at startup.
// 
// The appliocation demonstrates receiving data from the HX711 
// in either polled or interrupt driven mode. This is set by the 
// USE_INTERRUPT_MODE define (0 for polled, 1 for interrupt). To
// use interrupts the HX711 data line must be connected to a pin 
// supporting external interrupts.
//
// *** External Dependencies ***
// hd44780 at https://github.com/duinoWitchery/hd44780 or Arduino IDE Library Manager 
// MD_UISwitch at https://github.com/MajicDesigns/MD_UISwitch or the Arduino IDE LIbrary Manager
// 

#include <EEPROM.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include <MD_HX711.h>
#include <MD_UISwitch.h>

// Set to 0 for polled mode, 1 for interrupt mode.
// If interrupts are used then PIN_DAT must support 
// external interrupts
#define USE_INTERRUPT_MODE 1

#ifndef USE_DEBUG
#define USE_DEBUG 0   // set to 1 to turn on debug for this application
#endif

#if USE_DEBUG
#define DEBUGS(s)    do { Serial.print(F(s)); } while (false)
#define DEBUG(s, v)  do { Serial.print(F(s)); Serial.print(v); } while (false)
#define DEBUGX(s, v) do { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(v, HEX); } while (false)
#else
#define DEBUGS(s)
#define DEBUG(s, v)
#define DEBUGX(s, v)
#endif

const float CALIBRATE_WEIGHT = 1000.0;    // calibration weight in grams

// -- HX711
// Define pin connections to HX711 module
const uint8_t PIN_DAT = 2;
const uint8_t PIN_CLK = 4;

MD_HX711 scale(PIN_CLK, PIN_DAT);

// -- SWITCHES
// Define pin connections for switches
const uint8_t PIN_SCALE = 5;

MD_UISwitch_Digital swScale(PIN_SCALE);

// -- LCD DISPLAY
// Using I2C expander I/O
const uint8_t LCD_ADDR = 0x0;    // I2C address; set to 0 for autodetect
const uint8_t LCD_ROWS = 4;
const uint8_t LCD_COLS = 20;

hd44780_I2Cexp lcd(LCD_ADDR);

// Config Data Management ----------
class cConfig
{
public:
  int32_t tareValue;
  int32_t calibValue;

  cConfig(void) : tareValue(0), calibValue(0) {}

  void save(void)
  // Save the current config data
  {
    uint16_t addr = EEPROM_ADDR;

    DEBUGS("\nConfig Save");
    EEPROM.write(addr, SIG0);      addr += sizeof(SIG0);
    EEPROM.write(addr, SIG1);      addr += sizeof(SIG1);
    EEPROM.put(addr, tareValue);   addr += sizeof(tareValue);
    EEPROM.put(addr, calibValue);  addr += sizeof(calibValue);
  }

  bool load(void)
  {
    uint16_t addr = EEPROM_ADDR + sizeof(SIG0) + sizeof(SIG1);

    uint8_t s0 = EEPROM.read(EEPROM_ADDR);
    uint8_t s1 = EEPROM.read(EEPROM_ADDR + sizeof(SIG0));
    bool b = (s0 == SIG0 && s1 == SIG1);

    DEBUGX("\n\nSIG expect [", SIG0); DEBUGX(",", SIG1);
    DEBUGX("] got [", s0); DEBUGX(",", s1); DEBUGS("]");
    DEBUG("\nSIG match = ", (b ? 'T' : 'F'));

    if (b)
    {
      DEBUGS("\nConfig Load");
      EEPROM.get(addr, tareValue);   addr += sizeof(tareValue);
      EEPROM.get(addr, calibValue);  addr += sizeof(calibValue);
    }

    return(b);
  }

private:
  const uint16_t EEPROM_ADDR = 0;   // base address for load/save
  const uint8_t SIG0 = 0xe5;        // 2 signature bytes ..
  const uint8_t SIG1 = 0x5e;        // .. to detect config is valid
};

cConfig config;

float dampedWeight(float newReading)
// Dampen the fluctuations in readings value and make any small negative values 0.0
{
  const float PORTION = 0.80;
  static float lastReading = 0.0;

  // dampen
  lastReading += PORTION * (newReading - lastReading);

  // kill small negative values
  if (lastReading < 0.0 && lastReading > -0.1)
    lastReading = 0.0;

  return(lastReading);
}

void setup(void)
{
#if USE_DEBUG
  Serial.begin(57600);
#endif
  DEBUGS("\n[Weighscale Example]");

  // Switch
  swScale.begin();

  // LCD display
  int status = lcd.begin(LCD_COLS, LCD_ROWS);
  
  if (status) // non zero status means it was unsuccessful
  {
    DEBUGS("\nLCD failed startup");
    // hd44780 has a fatalError() routine that blinks an led if possible
    // begin() failed so blink error code using the on board LED if possible
    hd44780::fatalError(status); // does not return
  }

  // Configuration Data
  if (!config.load())
  {
    DEBUGS("\nInitialising Configuration");
    config.save();
  }
  DEBUG("\nTare Value:", config.tareValue);
  DEBUG("\nCalib Value:", config.calibValue);

  // Weigh Scale
  scale.begin();
  scale.setZeroTare(config.tareValue);
  scale.setCalibration(config.calibValue, CALIBRATE_WEIGHT);
#if USE_INTERRUPT_MODE
  scale.enableInterruptMode();
#endif
}

void loop(void)
{
  // Display current weight
#if USE_INTERRUPT_MODE
  static uint32_t lastCount = 0;

  if (scale.getReadCount() != lastCount)
  {
    lastCount = scale.getReadCount();
#else
  if (scale.isReady())
  {
    scale.read();
#endif
    lcd.setCursor(0, 0);
    lcd.print(dampedWeight(scale.getCalibrated()), 1);
    lcd.print("      ");  // clear following space of leftovers
    DEBUG("\nRaw value: ", scale.getRaw());
  }

  // Process UI switches
 switch (swScale.read())
  {
    case MD_UISwitch::KEY_PRESS:
    {
      lcd.setCursor(0, 1);
      lcd.print("Tare");
      scale.autoZeroTare();               // library work out tare value (blocking)
      config.tareValue = scale.getZeroTare();    // this is now the current value
      DEBUG("\nAuto Tare: ", config.tareValue);
      config.save();                       // save new config
      lcd.clear();
    }
    break;

    case MD_UISwitch::KEY_DPRESS:
    {
      config.calibValue = scale.getRaw();        // get the latest value
      DEBUG("\nNew Calibration: ", config.calibValue);
      scale.setCalibration(config.calibValue, CALIBRATE_WEIGHT);  // set the calibration using this value
      lcd.setCursor(0, 1);
      lcd.print("Calibrated");
      config.save();                       // save new config
      delay(500);
      lcd.clear();
    }

    default:  // keep the compiler from giving warnings
      break;
  }
}