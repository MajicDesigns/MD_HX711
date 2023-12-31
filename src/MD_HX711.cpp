﻿#include <MD_HX711.h>
/**
\page pageControl Hardware Control
## Sampling Rate
The HX711 can sample at 10 samples per second (SPS or Hz) or 80Hz, set by the
RATE pin on the IC. 

Some module boards have a jumper on the reverse side of the board to set the 
rate, as shown below. Connecting RATE to 1 (Vcc) sets 80Hz and 0 (GND) is 10Hz. 

\image{inline} html HX711_RATE.png "HX711 RATE jumper"
____

## Reset and Power Down Sequence
When power is first applied, the power on circuitry will reset the IC.

The CLK output from the processor is also used to reset the HX711 IC.
CLK should LOW by the processor unless clocking during a read cycle (see 
below).

When CLK is
- LOW, the HX711 is in normal working mode.
- changed from LOW to HIGH and stays HIGH for longer than 60μs, the HX711 
  is powered down. It remains in this state while the signal remains high.
- changed from HIGH to LOW, the HX711 resets and restarts normal working mode.

Following a reset the hardware defaults to Channel A input, gain 128.
____

## Data Read and Gain Control

\image{inline} html HX711_Serial_Interface.png "HX711 Serial Interface"

CLK is set by the processor, DAT is read by the processor. These two 
digital signals make up the serial interface to the HX711.

The data is read as 24 bits clocked out, one bit per clock cycle, from the 
HX711. Additional clock transitions (+1 to +3) are used to set the mode for 
the __next__ read cycle according to the handshaking sequence below.

- When an ADC reading is not available, DAT is set HIGH by the HX711. CLK is 
  held LOW by the processor.
- When DAT goes to LOW, the next ADC conversion can be read by the processor.
- The processor sends 25 to 27 positive (transition LOW to HIGH) CLK pulses 
  to shift the data out through DAT one bit per clock pulse, starting with the 
  MSB, until all 24 data bits are shifted out.
- The HX711 will then hold DAT HIGH for the remainder of the clock pulses.
- Channel and Gain selection for the __next__ read is controlled by the number 
  of additional CLK pulses (+1 to +3) send to the HX711, for a total of 25 to 
  27 clock pulses as shown in the table below.

Clock Pulses | Input Channel | Gain
------------:|:-------------:|-----:
     25      |       A       | 128
     26      |       B       |  32
     27      |       A       |  64

Fewer than 25 and more than 27 clock pulses in one communication cycle will 
cause a serial communication error requiring a HX711 reset.

\page pageLibrary Using the Library
The library supports a polled and an interrupt driven approach to obtaining 
data from the HX711.

The enableInterruptMode() method is used by the application to toggle interrupt 
mode on/off as needed.

In all modes, the last received data is buffered in the library and retrieved 
using the getRaw(), getTared() and getCalibrated() methods, depending on what 
data is needed by the application.

##Polled Mode
This is the default operating mode for the library.

In polled mode the read() method is invoked to read the next value from the 
hardware. If channel B is enabled the library will alternate reading channels 
(A, B, A, B, etc). The read() method always returns the id of the channel 
last processed.

As the read() method is blocking (it waits for the next data to be available 
read from the hardware) the isReady() method can be used to determine if  
there is data ready for processing before calling read().

## Interrupt Mode
Interrupt mode can be turned on and off as required using enableInterruptMode().

In interrupt mode the library will automatically process data received from
the HX711 based on an interrupt generated by the device DAT signal going low.
The application can monitor the getReadCount() method to determine when
new data has been received.

For interrupt mode to work the I/O pin connected to the DAT signal must support 
external interrupts (eg, for an Arduino Uno this would be pins 2 or 3).
*/

#include <MD_HX711.h>

/**
 * \file
 * \brief Code file for MD_HX711 library class
 */

#ifndef LIB_DEBUG
#define LIB_DEBUG 0      ///< 1 turns library debug output on
#endif

#if LIB_DEBUG
#define LIBPRINTS(s)    do { Serial.print(F(s)); } while (false)
#define LIBPRINT(s,v)   do { Serial.print(F(s)); Serial.print(v); } while (false)
#define LIBPRINTX(s,v)  do { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(v, HEX); } while (false)
#define LIBPRINTB(s,v)  do { Serial.print(F(s)); Serial.print(F("0b")); Serial.print(v, BIN); } while (false)
#else
#define LIBPRINTS(s)
#define LIBPRINT(s,v)
#define LIBPRINTX(s,v)
#define LIBPRINTB(s,v)
#endif

 // Interrupt handling declarations required outside the class
 // Global Data
static const uint8_t MAX_INSTANCE = 4;

uint8_t MD_HX711::ISRUsed = 0;                 // allocation bit field for the globalISRx()
MD_HX711* MD_HX711::myInstance[MAX_INSTANCE]; // callback instance handle for the ISR

// ISR for each myISRId
void MD_HX711::globalISR0(void) { MD_HX711::myInstance[0]->readNB(); }
void MD_HX711::globalISR1(void) { MD_HX711::myInstance[1]->readNB(); }
void MD_HX711::globalISR2(void) { MD_HX711::myInstance[2]->readNB(); }
void MD_HX711::globalISR3(void) { MD_HX711::myInstance[3]->readNB(); }


void MD_HX711::begin(void)
{
  LIBPRINTS("\nbegin()");
  pinMode(_pinClk, OUTPUT);
  pinMode(_pinDat, INPUT);

  if (isInterruptMode())
    disableISR();

  reset();
}

bool MD_HX711::enableInterruptMode(bool enable)
// Control the interrupt processing on and off.
{
  bool b = true;

  if (enable)
    b = enableISR();
  else
    disableISR();

  return(b);
}

bool MD_HX711::enableISR(void)
// Enable the ISR on this pin instance
{
  int8_t irq = digitalPinToInterrupt(_pinDat);

  LIBPRINTS("\nenableISR()");

  if (isInterruptMode()) return(false);

  // check if pin can be used for ISR
  if (irq != NOT_AN_INTERRUPT)
  {
    // assign ourselves a ISR ID ...
    for (uint8_t i = 0; i < MAX_INSTANCE; i++)
    {
      if (!(ISRUsed & _BV(i)))    // found a free ISR Id?
      {
        _myISRId = i;                // remember who this instance is
        myInstance[_myISRId] = this; // record this instance
        ISRUsed |= _BV(_myISRId);    // lock this in the allocations table
        break;
      }
    }
    // ... and attach corresponding ISR callback from the lookup table
    {
      static void((*ISRfunc[MAX_INSTANCE])(void)) =
      {
        globalISR0, globalISR1, globalISR2, globalISR3,
      };

      if (_myISRId != UINT8_MAX)   // we found one
      {
        LIBPRINT(" assigned ", _myISRId);
        attachInterrupt(irq, ISRfunc[_myISRId], LOW);
        powerDown();    // reset the hardware
        powerUp();
      }
      else
        irq = NOT_AN_INTERRUPT;
    }
  }

  return(irq != NOT_AN_INTERRUPT);
}

void MD_HX711::disableISR(void)
// Disable the ISR on this pin instance
// Shuffle the instance table entries up to squish out empty slots
{
  LIBPRINTS("\ndisableISR()");

  if (isInterruptMode())
  {
    LIBPRINTS(" stopping ISR");

    while (_inISR) { yield(); } ;   // wait for any current IRQ to stop

    noInterrupts();         // stop IRQs that may access this table while reorganizing

    detachInterrupt(digitalPinToInterrupt(_pinDat));
    ISRUsed &= ~_BV(_myISRId);   // free up the ISR slot for someone else

    interrupts();     // IRQs can access again
  }

  // reset global indicators
  _myISRId = UINT8_MAX;
  _inISR = false;
}

void MD_HX711::reset(void)
// set defaults and power cycle the hardware
{
  LIBPRINTS("\nreset()");
  powerDown();
  powerUp();

  // set the library defaults
  enableChannelB(false);
  setGainA(GAIN_128);
  disableISR();
  _nextReadA = true;
  _readCounter = 0;
  for (uint8_t ch = 0; ch < NUM_CHAN; ch++)
  {
    _chanData[ch].raw = 0;
    _chanData[ch].tare = 0;
    _chanData[ch].calib = 0;
    _chanData[ch].range = 0.0;
  }
}

inline void MD_HX711::powerDown(void)
// Set the CLK to low for at least 60us.
{
  LIBPRINTS("\npowerDown()");
  digitalWrite(_pinClk, HIGH);
  delayMicroseconds(64);   //  at least 60us HIGH
}

inline void MD_HX711::powerUp(void)
// set the CLK to high
{
  LIBPRINTS("\npowerUp()");
  digitalWrite(_pinClk, LOW);
}

void MD_HX711::autoZeroTare(void)
{
  const uint8_t NUM_PASSES = 3;

  LIBPRINTS("\nsetTare()");

  bool b = _enableB;      // remember this for later
  bool wasIRQ = isInterruptMode();

  // set the right environment
  if (wasIRQ) enableInterruptMode(false); // turn IRQ processing off
  enableChannelB(true);   // set up so we measure both channels

  // now take the average of NUM_PASSES readings for each channel
  for (int8_t n = 0; n < NUM_PASSES; n++)
  {
    for (int8_t i = 0; i < NUM_CHAN; i++)
    {
      channel_t ch = read();
      if (n == 0) 
        _chanData[ch].tare = _chanData[ch].raw;   // first time through just save the value
      else 
        _chanData[ch].tare = ((_chanData[ch].tare * (n - 1)) + _chanData[ch].raw) / n; // running average(ish)
    }
  }

  // now reset the environment to previous state
  enableChannelB(b);
  if (wasIRQ) enableInterruptMode(true);
}

float MD_HX711::getCalibrated(channel_t ch)
// retun value adjusted for tare and calibration
{
  float f;
/*
  LIBPRINT("\ngetCalbrated rng ", _chanData[ch].range);
  LIBPRINT(", raw ", _chanData[ch].raw);
  LIBPRINT(", cal ", _chanData[ch].calib);
  LIBPRINT(", tare ", _chanData[ch].tare);
*/
  if (_chanData[ch].calib - _chanData[ch].tare == 0)
    f = NAN;
  else
    f = _chanData[ch].range * (float(_chanData[ch].raw) - float(_chanData[ch].tare)) / (float(_chanData[ch].calib) - float(_chanData[ch].tare));

  return(f);
}

MD_HX711::channel_t MD_HX711::read(void)
// Blocking read. The method waits for the HX711 to tell us it has data.
// If operating in interrupt mode it immediately returns.
{
  LIBPRINTS("\nread()");

  // check for interrupt mode
  if (!isInterruptMode())
  {
    // blocking wait to make sure we have data to read
    while (!isReady()) { yield(); }

    // do the actual non-blocking read
    readNB();
  }

  // work out what channel we have just read to inform return code
  return(_enableB && _nextReadA ? CH_B : CH_A);
}

void MD_HX711::readNB(void)
// NON-Blocking read the data from the HX711 in an IRQ safe manner.
// Note: Only print debug if not IRQ processing 
{
  uint8_t extras = 0;
  int32_t value;

  _inISR = true;

  //if (isInterruptMode()) LIBPRINTS("\nreadNB()");

  // set the next read channel
  if (_enableB) _nextReadA = !_nextReadA;

  // now work out how many extra clock cycles send when reading data
  if (!_nextReadA)            extras = 2; // Channel B gain 32
  else if (_mode == GAIN_128) extras = 1; // Channel A gain 128
  else                        extras = 3; // Channel B gain 64

  //if (isInterruptMode()) LIBPRINT(" extras=", extras);

  // do the read
  noInterrupts();
  value = HX711ReadData(extras);
  interrupts();

  //if (isInterruptMode()) LIBPRINTX(" raw_value=", value);

  // sign extend the returned data
  if (value & 0x800000) value |= 0xff000000;

  //if (isInterruptMode()) LIBPRINTX(" ext_value=", value);

  // save the data to the right index value
  channel_t ch = (_enableB && _nextReadA) ? CH_B : CH_A;
  _chanData[ch].raw = value;

  // increment the counter
  _readCounter++;

  _inISR = false;
}

int32_t MD_HX711::HX711ReadData(uint8_t mode)
// read data and set the mode for next read depending on what 
// options are selected in the library
{
  // local variables are faster for pins
  uint8_t clk = _pinClk;
  uint8_t data = _pinDat;

  // data read controls
  int32_t value = 0;
  uint32_t mask = 0x800000L;
    
  do   // Read data bits from the HX711
  {
    digitalWrite(clk, HIGH);

    delayMicroseconds(1);   // T2 typ 1us

    if (digitalRead(data) == HIGH) value |= mask;
    digitalWrite(clk, LOW); 

    delayMicroseconds(1);  // T3 typ 1us
    mask >>= 1;
  } while (mask > 0);

  // Set the mode for the next read (just keep clocking)
  do
  {
    digitalWrite(clk, HIGH);
    delayMicroseconds(1);
    digitalWrite(clk, LOW);
    delayMicroseconds(1);
    mode--;
  } while (mode > 0);

  return(value);
}

