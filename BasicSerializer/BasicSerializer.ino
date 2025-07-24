// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       BasicSerializer.ino
    Created:	22.09.2024 21:09:56
    Author:     LAPTOP-8TKMEDGB\samue
*/

#include "./src/BasicSerializer.hpp"

using namespace halvoe;

const size_t bufferSize = 8;
std::array<uint8_t, bufferSize> buffer;

// The setup() function runs once each time the micro-controller starts
void setup()
{
  Serial.begin();
  while (not Serial);
  Serial.println("Enter setup...");

  // ---- Test: Serializer

  Serializer<bufferSize> serializer(buffer.data());
  serializer.write<uint8_t>(8);
  auto ref = serializer.skip<uint16_t>();
  if (ref.has_value())
  {
    ref.value().write(1024);
  }
  
  Serial.println(StatusPrinter::message(serializer.write<uint32_t>(32)));
  Serial.println(StatusPrinter::message(serializer.write<uint32_t>(32)));
  
  // ---- Test: Deserializer

  halvoe::Deserializer<bufferSize> deserializer(buffer.data());
  
  if (auto result = deserializer.read<uint8_t>(); result.has_value())
  {
    Serial.println(result.value());
  }
  else
  {
    Serial.println(StatusPrinter::message(result.error()));
  }

  if (auto result = deserializer.read<uint16_t>(); result.has_value())
  {
    Serial.println(result.value());
  }
  else
  {
    Serial.println(StatusPrinter::message(result.error()));
  }

  if (auto result = deserializer.read<uint32_t>(); result.has_value())
  {
    Serial.println(result.value());
  }
  else
  {
    Serial.println(StatusPrinter::message(result.error()));
  }

  if (auto result = deserializer.read<uint32_t>(); result.has_value())
  {
    Serial.println(result.value());
  }
  else
  {
    Serial.println(StatusPrinter::message(result.error()));
  }

  Serial.println("Leave setup...");
}

// Add the main program code into the continuous loop() function
void loop()
{

}
