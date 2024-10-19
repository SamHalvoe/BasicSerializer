// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       BasicSerializer.ino
    Created:	22.09.2024 21:09:56
    Author:     LAPTOP-8TKMEDGB\samue
*/

#include "BasicSerializer.hpp"

const size_t bufferSize = 256;
std::array<uint8_t, bufferSize> buffer;

// The setup() function runs once each time the micro-controller starts
void setup()
{
  halvoe::Serializer<bufferSize> serializer(buffer);
  halvoe::Deserializer<bufferSize> deserializer(buffer);
}

// Add the main program code into the continuous loop() function
void loop()
{

}
