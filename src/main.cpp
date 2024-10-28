#include "header.h"
#include "HomeSpan.h"

class DevLed : public Service::LightBulb
{
    int ledPin;
    SpanCharacteristic *power;

  public:
    DevLed(int ledPin)
    {
        power = new Characteristic::On();
        this->ledPin = ledPin;
        pinMode(ledPin, OUTPUT);
    }

    boolean update()
    {
        digitalWrite(ledPin, power->getNewVal());
        return (true);
    }
};

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);

    homeSpan.setControlPin(BUTTON_PIN);
    homeSpan.setStatusPin(LED_PIN);
    homeSpan.begin(Category::Outlets, "Moth Switch");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Manufacturer("MOTH");
    new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    new Characteristic::HardwareRevision(HARDWARE_VERSION);
    new Characteristic::Identify();

    new DevLed(IO_PIN);
}

void loop()
{
    homeSpan.poll();
}
