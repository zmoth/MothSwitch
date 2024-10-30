#include "header.h"
#include "HomeSpan.h"

class InvertedLED : public Blinkable
{
    int pin;

  public:
    InvertedLED(int pin) : pin{pin}
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    void on() override { digitalWrite(pin, LOW); }
    void off() override { digitalWrite(pin, HIGH); }
    int getPin() override { return (pin); }
};

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

    homeSpan.setStatusAutoOff(10);  // 10s
    homeSpan.setStatusDevice(new InvertedLED(LED_PIN));
    homeSpan.setControlPin(BUTTON_PIN);

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
