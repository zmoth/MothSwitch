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

    int resetCount{0};
    uint32_t resetAlarm{0};
    uint32_t resetTime{3000};

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
        unsigned long cTime = millis();

        if (resetCount == 0 || cTime > resetAlarm) {
            resetAlarm = cTime + resetTime;
            resetCount = 0;
        }

        if (pressType == SpanButton::OPEN || pressType == SpanButton::CLOSED) {
            power->setVal(1 - power->getVal());

            resetCount++;
        }

        if (resetCount == 7 && cTime < resetAlarm) {
            homeSpan.processSerialCommand("A");

            digitalWrite(ledPin, LOW);
            return;
        }

        digitalWrite(ledPin, power->getVal());
    }
};

String getID()
{
#if defined(ESP8266)
    String id(ESP.getChipId());
#elif defined(ESP32)
    String id((uint32_t)ESP.getEfuseMac(), HEX);
#endif
    id.toUpperCase();
    return id;
}

void setup()
{
    Serial.begin(115200);

    homeSpan.setStatusAutoOff(10);  // 10s
    homeSpan.setStatusDevice(new InvertedLED(LED_PIN));
    homeSpan.setControlPin(BUTTON_PIN);

    homeSpan.setApSSID("MothHomeSetup");
    homeSpan.setApPassword("97654321");

    homeSpan.begin(Category::Outlets, "Moth Switch");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Manufacturer("MOTH");
    new Characteristic::SerialNumber(getID().c_str());
    new Characteristic::Model("A1");
    new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    new Characteristic::HardwareRevision(HARDWARE_VERSION);
    new Characteristic::Identify();

    new DevLed(IO_PIN, KEY_PIN);
}

void loop()
{
    homeSpan.poll();
}
