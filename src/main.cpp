#include "header.h"
#include "HomeSpan.h"

class DevLed : public Service::LightBulb
{
    int ledPin;
    int powerPin;
    SpanCharacteristic *power;

  public:
    DevLed(int ledPin, int powerPin)
    {
        power = new Characteristic::On();
        this->ledPin = ledPin;
        pinMode(ledPin, OUTPUT);

        new SpanToggle(powerPin);
        this->powerPin = powerPin;
    }

    boolean update()
    {
        digitalWrite(ledPin, power->getNewVal());
        return (true);
    }

    void button(int pin, int pressType) override
    {
        if (pressType == SpanButton::OPEN || pressType == SpanButton::CLOSED) {
            power->setVal(1 - power->getVal());
        }

        digitalWrite(ledPin, power->getVal());
    }
};

void setup()
{
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

    new DevLed(IO_PIN, KEY_PIN);
}

void loop()
{
    homeSpan.poll();
}
