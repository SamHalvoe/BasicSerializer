// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       BasicSerializer.ino
    Created:	22.09.2024 21:09:56
    Author:     LAPTOP-8TKMEDGB\samue
*/

#include "./src/BasicSerializer.hpp"

const size_t bufferSize = 256;
std::array<uint8_t, bufferSize> buffer;

// The setup() function runs once each time the micro-controller starts
void setup()
{
  Serial.begin();
  while (not Serial);
  Serial.println("Enter setup...");

  halvoe::Serializer<bufferSize> serializer(buffer.data());
  serializer.write<uint8_t>(8);
  auto ref = serializer.skip<uint16_t>();
  if (ref.has_value())
  {
    ref.value().write(512);
  }

  halvoe::Deserializer<bufferSize> deserializer(buffer.data());
  Serial.println(deserializer.read<uint8_t>());
  Serial.println(deserializer.read<uint16_t>());

  Serial.println("Leave setup...");
}

// Add the main program code into the continuous loop() function
void loop()
{

}
