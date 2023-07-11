#pragma once
/**
\mainpage HX711 Weight Scales ADC library

A library to manage the configuration and reading of data of a HX711 
24 bit Analog to Digital Converter (ADC) for weight scale implementation.

This library implements a software interface to the HX711 hardware that enables:
- Channel A gain control selection. 
- Access to both ADC Channels A and B.
- Application can work in polled or interrupt data collection modes.
- Data retrieved as raw, tared or calibrated values.

\image{inline} html HX711_Module_PCB.jpg "HX711 Module"

More Information
- \subpage pageHardware
- \subpage pageControl
- \subpage pageLibrary

See Also
- \subpage pageRevisionHistory
- \subpage pageDonation
- \subpage pageCopyright

\page pageHardware Hardware
The HX711 is a precision 24-bit Analog to Digital Converter (ADC) designed 
for weigh scales and industrial control applications. It interfaces directly 
with a Wheatstone Bridge sensor.

\image{inline} html HX711_Typical_Circuit.png "Typical Application Circuit"

Channel A differential input is designed to interface with a bridge
sensor's differential output. It can be programmed with a gain of 128 
or 64. Large gains are needed to accommodate the small output signal 
from the sensor. When a 5V supply is used for the sensor, these gains 
correspond to a full-scale differential input voltage of +/-20mV or +/-40mV 
respectively.

Channel B differential input has a fixed gain of 32. The full-scale input 
voltage range is +/-80mV with a 5V power supply.

Each load cell has four strain gauges connected in a Wheatstone Bridge 
formation. The labeled colors correspond to the color coding convention 
coding of load cells. Red, black, green and white wires are connected to 
the load cell's strain gauge and yellow is an optional ground or shield 
wire to lessen EMI.

\image{inline} html HX711_Wheatstone_Bridge.jpg "Wheatstone Bridge"

The direction of the force arrow labels on your load cells must match the 
direction of resultant force in your application. So if your resultant 
force is straight down, then the load cells must be installed so that 
their force arrow labels are also pointing straight down.

\image{inline} html HX711_Load_Cell_Arrow.png "Load Cell Arrow Direction"
____

## Connections
The HX711 module is used to get measurable data out of a load cell and 
strain gauge. The load cells come in various shapes and are rated from 0.1kg 
to over 1000kg full scale.

The HX711 module's A channel interfaces to the load cell through five wires 
labeled E+ (or RED), E- (BLK), A-(WHT), A+(GRN), and optional SD(YLW) on 
the circuit board.

The B Channel interfaces to a separate load cell through the E+, E- 
(shared with Channel A) and B+, B- connections on the circuit board.

E+ and E- are essentially the positive voltage and ground connections to 
load cell; the A+/A-, B+/B- the differential outputs from the load cell.

The module is connected to the processor's power supply (Vcc and GND) and 2
additional processor digital input pins DAT (or DT) and CLK (or SCK). These 
digital pins are used to manage the limited configuration options available 
and read the data from the HX711. DAT and CLK are arbitrary and nominated to 
the library when the HX711 object is instantiated.

\image{inline} html HX711_Connections.png "HX711 Connections"

\page pageDonation Support the Library
If you like and use this library please consider making a small donation
using [PayPal](https://paypal.me/MajicDesigns/4USD)

\page pageCopyright Copyright
Copyright (C) 2023 Marco Colli. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

\page pageRevisionHistory Revision History
Jul 2023 ver 1.0.0
- Initial release
*/

#include <Arduino.h>

/**
 * \file
 * \brief Main header file and class definition for the MD_HX711 library.
 */

/**
 * Core object for the MD_HX711 library
 */
class MD_HX711
{
public:
 /**
  * Channel indicator enumerated type.
  *
  * This enumerated type is used as the return value from the read() call to 
  * indicate which channel was read during that call.
  */
  enum channel_t
  {
    CH_A = 0,   ///< Channel A indicator
    CH_B = 1    ///< Channel B indicator
  };

  /**
   * Gain indicator enumerated type.
   *
   * This enumerated type is used to set the required gain for Channel A.
   */
  enum mode_t
  {
    GAIN_128, ///< Channel A gain 128
    GAIN_64,  ///< Channel A gain 64
  };

  //--------------------------------------------------------------
  /** \name Class constructor and destructor.
   * @{
   */
  /**
   * Class Constructor
   *
   * Instantiate a new instance of the class.
   *
   * The main function for the core object is to set the internal
   * shared variables to default or passed values.
   *
   * \param pinClk pin used for SCK/CLK signal.
   * \param pinDat pin used for DAT/SD signal.
   */
    MD_HX711(uint8_t pinClk, uint8_t pinDat) :
        _pinClk(pinClk), _pinDat(pinDat), _myISRId(UINT8_MAX)
    {}
  
   /**
    * Class Destructor.
    *
    * Release any allocated memory and clean up anything else as required.
    */
     ~MD_HX711() { disableISR(); }

     /** @} */

  //--------------------------------------------------------------
  /** \name Methods for hardware control.
   * @{
   */
  /**
   * Initialize the object.
   *
   * Initialize the object data. This needs to be called during setup()
   * to set and initialize items that cannot be done during object creation.
   * 
   * A reset() is performed as part of this method.
   * 
   * \sa reset()
   */
    void begin(void);

  /**
   * Reset the HX711 hardware and library defaults.
   *
   * The HX711 Hardware is reset according to the protocol described in the data sheet
   * \ref pageControl. The library internal values and modes are also reset to default.
   *
   * \sa \ref pageControl
   */
    void reset(void);

  /**
   * Enable Channel B read cycle.
   *
   * Enable/disable Channel B reads. Channel B is not enabled by default.
   * 
   * When a read is performed, the return status will indicate which channel 
   * data was received from the HX711.
   *
   * \sa read()
   * 
   * \param ena set true to enable reads, false to disable.
   */
    inline void enableChannelB(bool ena = true) { _enableB = ena; }

  /**
    * Set Channel A gain.
    *
    * Set the gain for Channel A. The mode will change at the when Channel A is 
    * next scheduled for a read().
    * 
    * When a read is performed, the return status will indicate which channel
    * data was received from the HX711.
    *
    * \sa read(), mode_t
    *
    * \param mode set to one of the mode_t values.
    */
  inline void setGainA(mode_t mode) { _mode = mode; }


    /** @} */

  //--------------------------------------------------------------
  /** \name Methods for Data management.
   * @{
   */

   /**
    * Return status of data ready at HX711
    *
    * Return boolean status of whether converted data is ready at the HX711.
    *
    * \return true if data is ready at the HX711 ADC
    */
  inline bool isReady(void) { return (digitalRead(_pinDat) == LOW); }

  /**
   * Read next data from the HX711 device.
   *
   * Wait for the data to be ready at the device and then read the data into the
   * registers. This method is most useful for polled operation.
   *
   * If the library is operating in interrupt mode then this immediately returns
   * the channel that was last received from the HX711.
   *
   * If a non-blocking read is required, the application monitor using isReady()
   * and only call this method when there is data available at the HX711.
   *
   * \sa isReady(), enableInterruptMode(), channel_t
   *
   * \return an indicator of which channel was last read
   */
  channel_t read(void);

  /**
    * Set Tare offset for all channels from current readings.
    *
    * Set the tare offset for both channels A and B by using the current values 
    * returned from the HX711. This method averages of a small number of values 
    * received using the the read() method and will block until the tare operation 
    * is completed.
    * 
    * If interrupt mode is enabled, the mode it will be turned off while this 
    * operation takes place and turned back on when completed.
    *
    * \sa read(), autoZeroTare(), getZeroTare()
    */
  void autoZeroTare(void);

  /**
    * Set Tare offset for specific channel to a value.
    *
    * Set the tare offset for the channel specified to the value specified.
    *
    * \sa read(), autoZeroTare(), getZeroTare()
    * 
    * \param tare the tare offset value
    * \param ch   the channel to which this applies. Default channel is CH_A.
    */
  inline void setZeroTare(int32_t tare, channel_t ch = CH_A) { _chanData[ch].tare = tare; }

  /**
    * Set Calibration for specific channel.
    *
    * Set the weight calibration for the channel specified to the value specified.
    * The value will be associated with the full scale calibration value.
    *
    * \sa autoZeroTare(), read()
    *
    * \param value the ADC value for the range specified
    * \param range the value to be associated with the ADC calibration value
    * \param ch    the channel to which this applies. Default channel is CH_A.
    */
  inline void setCalibration(int32_t value, float range, channel_t ch = CH_A) { _chanData[ch].calib = value; _chanData[ch].range = range; }

  /**
    * Get the current tare offset.
    *
    * Get the current tare offset for the specified channel.
    *
    * \sa autoZeroTare(), setZeroTare()
    * 
    * \param ch the channel of interest. Default channel is CH_A.
    * \return the requested tare offset value
    */
  inline int32_t getZeroTare(channel_t ch = CH_A) { return(_chanData[ch].tare); }

  /**
    * Get the current calibration value.
    *
    * Get the current calibration value for the specified channel.
    *
    * \sa setCalibration()
    *
    * \param ch the channel of interest. Default channel is CH_A.
    * \return the requested tare offset value
    */
  inline int32_t getCalibration(channel_t ch = CH_A) { return(_chanData[ch].calib); }

  /**
    * Get raw data.
    *
    * Get the last raw data processed for the specified channel.
    *
    * \sa read()
    *
    * \param ch the channel of interest. Default channel is CH_A.
    * \return the requested raw value
    */
  inline int32_t getRaw(channel_t ch = CH_A) { return(_chanData[ch].raw); }

  /**
    * Get tared data.
    *
    * Get the latest raw data adjusted for the tare for the specified channel.
    *
    * \sa read(), autoZeroTare(), setZeroTare()
    *
    * \param ch the channel of interest. Default channel is CH_A.
    * \return the requested tare adjusted value
    */
  inline int32_t getTared(channel_t ch = CH_A) { return(_chanData[ch].raw - _chanData[ch].tare); }

  /**
    * Get calibrated data.
    *
    * Get the latest raw data adjusted for tare and proportioned to the calibrated value.
    *
    * \sa read(), autoZeroTare(), setZeroTare(), setCalibration()
    *
    * \param ch the channel of interest. Default channel is CH_A.
    * \return the requested calibration adjusted value. If the calibration is not set returns NAN
    */
  float getCalibrated(channel_t ch = CH_A);
    
  /**
    * Get cumulative read count.
    *
    * The read count is increment by 1 each time that a reading is received from the 
    * HX711 for either channel. This method allows the application to determine when 
    * new data is received and is useful when processing data in interrupt mode.
    *
    * \sa read(), enableInterruptMode()
    *
    * \return the requested calibration adjusted value. If the calibration is not set returns NAN
    */
  uint32_t getReadCount(void) { return(_readCounter); }
  
  /** @} */

  //--------------------------------------------------------------
  /** \name Interrupt Mode management.
    * @{
    */
  /**
    * Control the interrupt mode.
    *
    * Turn interrupt mode operation on or off as required.
    * 
    * When using interrupt mode the data received is buffered by the library. The 
    * application can use changes in getReadCount() to determine when a new reading 
    * has been received from the HX711 hardware.
    * 
    * For interrupt mode to work I/O pin connected to the HX711 data output must 
    * support external interrupts
    * 
    * Interrupt mode also changes the way that read() behaves as documented 
    * for that method.
    *
    * \sa getReadCount(), read()
    *
    * \param enable if true enables the interrupt processing, false disables processing. Default is true.
    * \return true if the operation successfully completed and interrupt mode is enabled.
    */
  bool enableInterruptMode(bool enable = true);

   /**
    * Current interrupt mode status
    * 
    * Return what the current operating mode is.
    * 
    * \sa enableInterruptMode()
    * 
    * \return true if currently operating in interrupt mode, false otherwise
    */
  inline bool isInterruptMode(void) { return(_myISRId != UINT8_MAX); }
  
  /** @} */


private:
  static const uint8_t NUM_CHAN = 2;

  typedef struct
  {
    volatile int32_t raw;    ///< raw data for Channels A/B
    int32_t tare;   ///< the tare offset
    int32_t calib;  ///< the calibration value for range
    float range;    ///< the range value for the calibration
  } channelInfo_t;

  uint8_t _pinClk;   ///< clock pin number
  uint8_t _pinDat;   ///< data pin number

  // all variables in this section must be initialized in reset()
  bool    _enableB;       ///< channel B read enabled when true
  mode_t  _mode;          ///< channel A current mode
  bool    _nextReadA;     ///< if true read channel A next. Used to set the mode in the HX711.
  volatile uint32_t _readCounter;    ///< count the number of times the HX711 has been accessed
  channelInfo_t _chanData[NUM_CHAN];  ///< channel related data

  // Private helper methods
  void powerDown(void);   ///< power down the HX711
  void powerUp(void);     ///< power up the HX711
  int32_t HX711ReadData(uint8_t mode);   ///< read data from HX711 and set extra mode bits

  // ISR related private data
  uint8_t _myISRId;       ///< my instance ISR Id for myInstance[x] and global ISRx
  bool _inISR;            ///< set true when currently processing ISR

  // IRQ related global data
  static uint8_t ISRUsed;        ///< Keep track of which ISRs are used (global bit field)
  static MD_HX711* myInstance[]; ///< Callback instance for the ISR to reach instanceISR()

  // IRQ support functions 
  bool enableISR(void);   ///< Attach the ISR and start processing as interrupts
  void disableISR(void);  ///< Detach the ISR and stop processing interrupts
  void readNB(void);      ///< Non-blocking read the HX711 in IRQ safe mode

  // Prototype all the [MAX_INSTANCE] encoder ISRs
  static void globalISR0(void);
  static void globalISR1(void);
  static void globalISR2(void);
  static void globalISR3(void);
};
