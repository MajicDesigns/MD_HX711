// Simplest use of the library
//
// - Read default Channel A and B
// - Display the raw data to serial monitor
//

#include <MD_HX711.h>

// Define pin connections to HX711 module
const uint8_t PIN_DAT = 2;
const uint8_t PIN_CLK = 4;

MD_HX711 scale(PIN_CLK, PIN_DAT);

void setup(void)
{
  Serial.begin(57600);
  Serial.println("[MD_HX711 Simple]");

  scale.begin();
  scale.enableChannelB();
}

void loop(void)
{
  MD_HX711::channel_t ch = scale.read();

  Serial.print(ch == MD_HX711::CH_A ? "A: " : "B: ");
  Serial.println(scale.getRaw(ch));
}