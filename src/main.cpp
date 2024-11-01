#include "header.h"
#include "HomeSpan.h"

class InvertedLED : public Blinkable
{
    int pin;

  public:
    InvertedLED(int pin) : pin(pin)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    void on() override { digitalWrite(pin, LOW); }
    void off() override { digitalWrite(pin, HIGH); }
    int getPin() override { return (pin); }
};

class DevOutlet : public Service::Outlet
{
    int outPin;
    int powerPin;
    SpanCharacteristic *power{NULL};

    int resetCount{0};
    uint32_t resetAlarm{0};
    uint32_t resetTime{3000};

  public:
    DevOutlet(int outPin, int powerPin) : outPin(outPin), powerPin(powerPin)
    {
        power = new Characteristic::On();
        pinMode(outPin, OUTPUT);

        new SpanToggle(powerPin);
    }

    boolean update() override
    {
        digitalWrite(outPin, power->getNewVal());
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

            digitalWrite(outPin, LOW);
            return;
        }

        digitalWrite(outPin, power->getVal());
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

    homeSpan.setPairingCode("97654321");
    homeSpan.setApSSID("MothHomeSetup");
    homeSpan.setApPassword("97654321");
    homeSpan.enableOTA("97654321");

    homeSpan.setStatusAutoOff(10);  // 10s
    homeSpan.setStatusDevice(new InvertedLED(LED_PIN));
    homeSpan.setControlPin(BUTTON_PIN);

    homeSpan.begin(Category::Outlets, "Moth Switch");

    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Manufacturer("MOTH");
    new Characteristic::SerialNumber(getID().c_str());
    new Characteristic::Model("A1");
    new Characteristic::FirmwareRevision(FIRMWARE_VERSION);
    new Characteristic::HardwareRevision(HARDWARE_VERSION);
    new Characteristic::Identify();

    new DevOutlet(IO_PIN, KEY_PIN);
}

void loop()
{
    homeSpan.poll();
}
