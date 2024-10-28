#include <Arduino.h>

void setup()
{
    Serial.begin(115200);  // 设置波特率为115200
}

void loop()
{
    Serial.print("hello world");
    delay(1000);
}
