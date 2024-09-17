#include <Ps3Controller.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("Ready.");
    Ps3.begin("98:b6:58:30:04:ae");
    Serial.println("Ready.");
}

void loop()
{
  if (Ps3.isConnected()){
    Serial.println("Connected!");
  }

  delay(3000);
}
