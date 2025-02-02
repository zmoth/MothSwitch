/*********************************************************************************
 *  MIT License
 *
 *  Copyright (c) 2020-2024 Gregg E. Berman
 *
 *  https://github.com/HomeSpan/HomeSpan
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 ********************************************************************************/

#pragma once

#include "version.h"

// eliminates warning messages from use of pointers to member functions to detect whether update() and loop() are
// overridden by user
#pragma GCC diagnostic ignored "-Wpmf-conversions"

// eliminates warning message regarded unused result from call to crypto_scalarmult_curve25519()
#pragma GCC diagnostic ignored "-Wunused-result"

#include <Arduino.h>
#include <unordered_map>
#include <vector>
#include <list>
#include <nvs.h>
#include <ArduinoOTA.h>
#include <esp_now.h>
#include <mbedtls/base64.h>

#include "src/extras/Blinker.h"
#include "src/extras/Pixel.h"
#include "src/extras/RFControl.h"
#include "src/extras/PwmPin.h"
#include "src/extras/StepperControl.h"

#include "Settings.h"
#include "Utils.h"
#include "Network.h"
#include "HAPConstants.h"
#include "HapQR.h"
#include "Characteristics.h"
#include "TLV8.h"

using std::list;
using std::unordered_map;
using std::vector;

enum
{
    GET_AID = 1,
    GET_META = 2,
    GET_PERMS = 4,
    GET_TYPE = 8,
    GET_EV = 16,
    GET_DESC = 32,
    GET_NV = 64,
    GET_VALUE = 128,
    GET_STATUS = 256
};

typedef boolean BOOL_t;
typedef uint8_t UINT8_t;
typedef uint16_t UINT16_t;
typedef uint32_t UINT32_t;
typedef uint64_t UINT64_t;
typedef int32_t INT_t;
typedef double FLOAT_t;
typedef const char *STRING_t;
typedef const TLV8 &TLV_ENC_t;
typedef std::pair<const uint8_t *, size_t> DATA_t;

static DATA_t NULL_DATA = {NULL, 0};
static TLV8 NULL_TLV{};

///////////////////////////////

#define STATUS_UPDATE(LED_UPDATE, MESSAGE_UPDATE)    \
    {                                                \
        homeSpan.statusLED->LED_UPDATE;              \
        if (homeSpan.statusCallback)                 \
            homeSpan.statusCallback(MESSAGE_UPDATE); \
    }

enum HS_STATUS
{
    HS_WIFI_NEEDED,      // WiFi Credentials have not yet been set/stored
    HS_WIFI_CONNECTING,  // HomeSpan is trying to connect to the network specified in the stored WiFi Credentials
    HS_PAIRING_NEEDED,   // HomeSpan is connected to central WiFi network, but device has not yet been paired to HomeKit
    HS_PAIRED,           // HomeSpan is connected to central WiFi network and ther device has been paired to HomeKit
    HS_ENTERING_CONFIG_MODE,            // User has requested the device to enter into Command Mode
    HS_CONFIG_MODE_EXIT,                // HomeSpan is in Command Mode with "Exit Command Mode" specified as choice
    HS_CONFIG_MODE_REBOOT,              // HomeSpan is in Command Mode with "Reboot" specified as choice
    HS_CONFIG_MODE_LAUNCH_AP,           // HomeSpan is in Command Mode with "Launch Access Point" specified as choice
    HS_CONFIG_MODE_UNPAIR,              // HomeSpan is in Command Mode with "Unpair Device" specified as choice
    HS_CONFIG_MODE_ERASE_WIFI,          // HomeSpan is in Command Mode with "Erase WiFi Credentials" specified as choice
    HS_CONFIG_MODE_EXIT_SELECTED,       // User has selected "Exit Command Mode"
    HS_CONFIG_MODE_REBOOT_SELECTED,     // User has select "Reboot" from the Command Mode
    HS_CONFIG_MODE_LAUNCH_AP_SELECTED,  // User has selected "Launch AP Access" from the Command Mode
    HS_CONFIG_MODE_UNPAIR_SELECTED,     // User has seleected "Unpair Device" from the Command Mode
    HS_CONFIG_MODE_ERASE_WIFI_SELECTED,  // User has selected "Erase WiFi Credentials" from the Command Mode
    HS_REBOOTING,                        // HomeSpan is in the process of rebooting the device
    HS_FACTORY_RESET,                    // HomeSpan is in the process of performing a Factory Reset of device
    HS_AP_STARTED,                       // HomeSpan has started the Access Point but no one has yet connected
    HS_AP_CONNECTED,                     // The Access Point is started and a user device has been connected
    HS_AP_TERMINATED,                    // HomeSpan has terminated the Access Point
    HS_OTA_STARTED                       // HomeSpan is in the process of recveived an Over-the-Air software update
};

///////////////////////////////

// Forward-Declarations

struct Span;
struct SpanAccessory;
struct SpanService;
struct SpanCharacteristic;
struct SpanBuf;
struct SpanButton;
struct SpanUserCommand;

struct HAPClient;
class Controller;

extern Span homeSpan;

////////////////////////////////////////////////////////
// INTERNAL HOMESPAN STRUCTURES - NOT FOR USER ACCESS //
////////////////////////////////////////////////////////

struct SpanPartition
{
    char magicCookie[32];
    uint8_t reserved[224];
};

///////////////////////////////

struct SpanConfig
{
    int configNumber = 0;        // configuration number - broadcast as Bonjour "c#" (computed automatically)
    uint8_t hashCode[48] = {0};  // SHA-384 hash of Span Database stored as a form of unique "signature" to know when to
                                 // update the config number upon changes
};

///////////////////////////////

// temporary storage buffer for use with putCharacteristicsURL() and checkTimedResets()
struct SpanBuf
{
    uint32_t aid = 0;    // updated aid
    uint32_t iid = 0;    // updated iid
    boolean wr = false;  // flag to indicate write-response has been requested
    // updated value (optional, though either at least 'val' or 'ev' must be specified)
    char *val = NULL;
    // updated event notification flag (optional, though either at least 'val' or 'ev' must be specified)
    char *ev = NULL;
    StatusCode status;                          // return status (HAP Table 6-11)
    SpanCharacteristic *characteristic = NULL;  // Characteristic to update (NULL if not found)
};

///////////////////////////////

struct SpanWebLog
{                                   // optional web status/log data
    boolean isEnabled = false;      // flag to inidicate WebLog has been enabled
    uint16_t maxEntries = 0;        // max number of log entries;
    int nEntries = 0;               // total cumulative number of log entries
    const char *timeServer = NULL;  // optional time server to use for acquiring clock time
    const char *timeZone;           // optional time-zone specification
    boolean timeInit = false;       // flag to indicate time has been initialized
    char bootTime[33] = "Unknown";  // boot time
    String statusURL;               // URL of status log
    uint32_t waitTime = 120000;     // number of milliseconds to wait for initial connection to time server
    String css = "";                // optional user-defined style sheet for web log

    struct log_t
    {                         // log entry type
        uint64_t upTime;      // number of seconds since booting
        struct tm clockTime;  // clock time
        char *message;        // pointers to log entries of arbitrary size
        String clientIP;      // IP address of client making request (or "0.0.0.0" if not applicable)
    } *log = NULL;            // array of log entries

    void init(uint16_t maxEntries, const char *serv, const char *tz, const char *url);
    static void initTime(void *args);
    void vLog(boolean sysMsg, const char *fmr, va_list ap);
};

///////////////////////////////

struct SpanOTA
{  // manages OTA process

    char otaPwd[33] = "";  // MD5 Hash of OTA password, represented as a string of hexidecimal characters

    static boolean enabled;  // enables OTA - default if not enabled
    static boolean auth;     // indicates whether OTA password is required
    static int otaPercent;
    static boolean safeLoad;  // indicates whether OTA update should reject any application update that is not another
                              // HomeSpan sketch

    int init(boolean auth, boolean safeLoad, const char *pwd);
    int setPassword(const char *pwd);
    static void start();
    static void end();
    static void progress(uint32_t progress, uint32_t total);
    static void error(ota_error_t err);
};

//////////////////////////////////////
//   USER API CLASSES BEGINS HERE   //
//////////////////////////////////////

class Span
{
    friend class SpanAccessory;
    friend class SpanService;
    friend class SpanCharacteristic;
    friend class SpanUserCommand;
    friend class SpanButton;
    friend class SpanWebLog;
    friend class SpanOTA;
    friend class Network;
    friend class HAPClient;

    char *displayName;   // display name for this device - broadcast as part of Bonjour MDNS
    char *hostNameBase;  // base of hostName of this device - full host name broadcast by Bonjour MDNS will have 6-byte
                         // accessoryID as well as '.local' automatically appended
    char *hostNameSuffix = NULL;  // optional "suffix" of hostName of this device.  If specified, will be used as the
                                  // hostName suffix instead of the 6-byte accessoryID
    char *hostName = NULL;        // derived full hostname
    char *modelName;              // model name of this device - broadcast as Bonjour field "md"
    char category[3] = "";        // category ID of primary accessory - broadcast as Bonjour field "ci" (HAP Section 13)
    unsigned long snapTime;       // current time (in millis) snapped before entering Service loops() or updates()
    boolean isInitialized = false;  // flag indicating HomeSpan has been initialized
    boolean isBridge = true;  // flag indicating whether device is configured as a bridge (i.e. first Accessory contains
                              // nothing but AccessoryInformation and HAPProtocolInformation)
    HapQR qrCode;             // optional QR Code to use for pairing
    const char *sketchVersion = "n/a";  // version of the sketch
    char pairingCodeCommand[12] = "";   // user-specified Pairing Code - only needed if Pairing Setup Code is specified
                                        // in sketch using setPairingCode()
    String lastClientIP = "0.0.0.0";    // IP address of last client accessing device through encrypted channel
    boolean newCode;  // flag indicating new application code has been loaded (based on keeping track of app SHA256)
    boolean serialInputDisabled = false;  // flag indiating that serial input is disabled
    uint8_t rebootCount = 0;      // counts number of times device was rebooted (used in optional Reboot callback)
    uint32_t rebootCallbackTime;  // length of time to wait (in milliseconds) before calling optional Reboot callback

    nvs_handle charNVS;      // handle for non-volatile-storage of Characteristics data
    nvs_handle wifiNVS = 0;  // handle for non-volatile-storage of WiFi data
    nvs_handle otaNVS;       // handle for non-volatile storage of OTA data
    nvs_handle srpNVS;       // handle for non-volatile storage of SRP data
    nvs_handle hapNVS;       // handle for non-volatile-storage of HAP data

    int connected = 0;               // WiFi connection status (increments upon each connect and disconnect)
    unsigned long waitTime = 60000;  // time to wait (in milliseconds) between WiFi connection attempts
    unsigned long alarmConnect = 0;  // time after which WiFi connection attempt should be tried again

    const char *defaultSetupCode = DEFAULT_SETUP_CODE;  // Setup Code used for pairing
    uint16_t autoOffLED = 0;                            // automatic turn-off duration (in seconds) for Status LED
    int logLevel = DEFAULT_LOG_LEVEL;                   // level for writing out log messages to serial monitor
    unsigned long comModeLife =
        DEFAULT_COMMAND_TIMEOUT *
        1000;  // length of time (in milliseconds) to keep Command Mode alive before resuming normal operations
    uint16_t tcpPortNum = DEFAULT_TCP_PORT;  // port for TCP communications between HomeKit and HomeSpan
    char qrID[5] = "";                       // Setup ID used for pairing with QR Code
    // optional callback function to invoke once WiFi connectivity is initially established
    void (*wifiCallback)() = NULL;
    // optional callback function to invoke every time WiFi connectivity is established or re-established
    void (*wifiCallbackAll)(int) = NULL;
    // optional callback function to invoke after header table in Web Log is produced
    void (*weblogCallback)(String &) = NULL;
    // optional callback function to invoke when pairing is established (true) or lost (false)
    void (*pairCallback)(boolean isPaired) = NULL;
    boolean autoStartAPEnabled = false;  // enables auto start-up of Access Point when WiFi Credentials not found
    void (*apFunction)() = NULL;         // optional function to invoke when starting Access Point
    void (*statusCallback)(HS_STATUS status) = NULL;  // optional callback when HomeSpan status changes
    void (*rebootCallback)(uint8_t) = NULL;           // optional callback when device reboots
    void (*controllerCallback)() = NULL;              // optional callback when Controller is added/removed/changed

    WiFiServer *hapServer;             // pointer to the HAP Server connection
    Blinker *statusLED;                // indicates HomeSpan status
    Blinkable *statusDevice = NULL;    // the device used for the Blinker
    PushButton *controlButton = NULL;  // controls HomeSpan configuration and resets
    Network network;    // configures WiFi and Setup Code via either serial monitor or temporary Access Point
    SpanWebLog webLog;  // optional web status/log
    TaskHandle_t pollTaskHandle = NULL;   // optional task handle to use for poll() function
    TaskHandle_t loopTaskHandle;          // Arduino Loop Task handle
    boolean verboseWifiReconnect = true;  // set to false to not print WiFi reconnect attempts messages

    SpanOTA spanOTA;       // manages OTA process
    SpanConfig hapConfig;  // track configuration changes to the HAP Accessory database; used to increment the
                           // configuration number (c#) when changes found

    list<HAPClient, Mallocator<HAPClient>> hapList;  // linked-list of HAPClient structures containing HTTP client
                                                     // connections, parsing routines, and state variables
    list<HAPClient, Mallocator<HAPClient>>::iterator currentClient;    // iterator to current client
    vector<SpanAccessory *, Mallocator<SpanAccessory *>> Accessories;  // vector of pointers to all Accessories
    vector<SpanService *, Mallocator<SpanService *>>
        Loops;  // vector of pointer to all Services that have over-ridden loop() methods
    vector<SpanBuf, Mallocator<SpanBuf>>
        Notifications;  // vector of SpanBuf objects that store info for Characteristics that are updated with setVal()
                        // and require a Notification Event
    vector<SpanButton *, Mallocator<SpanButton *>> PushButtons;  // vector of pointer to all PushButtons
    unordered_map<uint64_t, uint32_t> TimedWrites;        // map of timed-write PIDs and Alarm Times (based on TTLs)
    unordered_map<char, SpanUserCommand *> UserCommands;  // map of pointers to all UserCommands

    void pollTask();      // poll HAP Clients and process any new HAP requests
    void checkConnect();  // check WiFi connection; connect if needed
    void commandMode();   // allows user to control and reset HomeSpan settings with the control button
    void resetStatus();   // resets statusLED and calls statusCallback based on current HomeSpan status
    void reboot();        // reboots device

    // writes Attributes JSON database to hapOut stream
    void printfAttributes(int flags = GET_VALUE | GET_META | GET_PERMS | GET_TYPE | GET_DESC);

    // return Characteristic with matching aid and iid (else NULL if not found)
    SpanCharacteristic *find(uint32_t aid, uint32_t iid);
    // return number of characteristic objects referenced in PUT /characteristics JSON request
    int countCharacteristics(char *buf);
    // parses PUT /characteristics JSON request 'buf into 'pObj' and updates referenced characteristics; returns 1 on
    // success, 0 on fail
    int updateCharacteristics(char *buf, SpanBuf *pObj);
    // writes SpanBuf objects to hapOut stream
    void printfAttributes(SpanBuf *pObj, int nObj);
    // writes accessory requested characteristic ids to hapOut stream - returns true if all characteristics are found
    // and readable, else returns false
    boolean printfAttributes(char **ids, int numIDs, int flags);
    // clear all notifications related to specific client connection
    void clearNotify(HAPClient *hc);
    // writes notification JSON to hapOut stream based on SpanBuf objects and specified connection
    void printfNotify(SpanBuf *pObj, int nObj, HAPClient *hc);

    static boolean invalidUUID(const char *uuid)
    {
        int x = 0;
        sscanf(uuid, "%*8[0-9a-fA-F]%n", &x);  // check for short-form of UUID
        if (strlen(uuid) == x && uuid[0] != '0')
            return (false);
        sscanf(uuid, "%*8[0-9a-fA-F]-%*4[0-9a-fA-F]-%*4[0-9a-fA-F]-%*4[0-9a-fA-F]-%*12[0-9a-fA-F]%n", &x);
        return (strlen(uuid) != 36 || x != 36);
    }

  public:
    Span();  // constructor

    void begin(Category catID = DEFAULT_CATEGORY,
               const char *displayName = DEFAULT_DISPLAY_NAME,
               const char *hostNameBase = DEFAULT_HOST_NAME,
               const char *modelName = DEFAULT_MODEL_NAME);

    // calls pollTask() with some error checking
    void poll();
    // process command 'c' (typically from readSerial, though can be called with any 'c')
    void processSerialCommand(const char *c);

    // updates HAP Configuration Number and Loop vector; if updateMDNS=true and config number has changed, re-broadcasts
    // MDNS 'c#' record; returns true if config number changed
    boolean updateDatabase(boolean updateMDNS = true);
    // deletes Accessory with matching aid; returns true if found, else returns false
    boolean deleteAccessory(uint32_t aid);

    // sets Control Pin, with optional trigger type
    Span &setControlPin(uint8_t pin, PushButton::triggerType_t triggerType = PushButton::TRIGGER_ON_LOW)
    {
        controlButton = new PushButton(pin, triggerType);
        return (*this);
    }
    // get Control Pin (returns -1 if undefined)
    int getControlPin() { return (controlButton ? controlButton->getPin() : -1); }
    // sets Status Device to a simple LED on specified pin
    Span &setStatusPin(uint8_t pin)
    {
        statusDevice = new GenericLED(pin);
        return (*this);
    }
    // sets Status Device to an RGB Pixel on specified pin
    Span &setStatusPixel(uint8_t pin, float h = 0, float s = 100, float v = 100)
    {
        statusDevice = ((new Pixel(pin))->setOnColor(Pixel::HSV(h, s, v)));
        return (*this);
    }
    // sets Status Device to a generic Blinkable object
    Span &setStatusDevice(Blinkable *sDev)
    {
        statusDevice = sDev;
        return (*this);
    }
    // sets Status LED auto off (seconds)
    Span &setStatusAutoOff(uint16_t duration)
    {
        autoOffLED = duration;
        return (*this);
    }
    // get Status Pin (returns -1 if undefined)
    int getStatusPin() { return (statusLED->getPin()); }
    // refreshes state of Status LED
    void refreshStatusDevice()
    {
        if (statusLED)
            statusLED->refresh();
    }
    // sets Access Point SSID
    Span &setApSSID(const char *ssid)
    {
        network.apSSID = ssid;
        return (*this);
    }
    // sets Access Point Password
    Span &setApPassword(const char *pwd)
    {
        network.apPassword = pwd;
        return (*this);
    }
    // sets Access Point Timeout (seconds)
    Span &setApTimeout(uint16_t nSec)
    {
        network.lifetime = nSec * 1000;
        return (*this);
    }
    // sets Command Mode Timeout (seconds)
    Span &setCommandTimeout(uint16_t nSec)
    {
        comModeLife = nSec * 1000;
        return (*this);
    }
    // sets Log Level for log messages (0=baseline, 1=intermediate, 2=all, -1=disable all serial input/output)
    Span &setLogLevel(int level)
    {
        logLevel = level;
        return (*this);
    }
    // get Log Level
    int getLogLevel() { return (logLevel); }
    // sets whether serial input is disabled (true) or enabled (false)
    Span &setSerialInputDisable(boolean val)
    {
        serialInputDisabled = val;
        return (*this);
    }
    // returns true if serial input is disabled, or false if serial input in enabled
    boolean getSerialInputDisable() { return (serialInputDisabled); }
    // sets the TCP port number to use for communications between HomeKit and HomeSpan
    Span &setPortNum(uint16_t port)
    {
        tcpPortNum = port;
        return (*this);
    }
    // sets the Setup ID for optional pairing with a QR Code
    Span &setQRID(const char *id);
    // set optional sketch version number
    Span &setSketchVersion(const char *sVer)
    {
        sketchVersion = sVer;
        return (*this);
    }
    // get sketch version number
    const char *getSketchVersion() { return sketchVersion; }
    // sets an optional user-defined function to call once WiFi connectivity is initially established
    Span &setWifiCallback(void (*f)())
    {
        wifiCallback = f;
        return (*this);
    }
    // sets an optional user-defined function to call every time WiFi connectivity is established or re-established
    Span &setWifiCallbackAll(void (*f)(int))
    {
        wifiCallbackAll = f;
        return (*this);
    }
    // sets an optional user-defined function to call when Pairing is established (true) or lost (false)
    Span &setPairCallback(void (*f)(boolean isPaired))
    {
        pairCallback = f;
        return (*this);
    }
    // sets an optional user-defined function to call when activating the WiFi Access Point
    Span &setApFunction(void (*f)())
    {
        apFunction = f;
        return (*this);
    }
    // enables auto start-up of Access Point when WiFi Credentials not found
    Span &enableAutoStartAP()
    {
        autoStartAPEnabled = true;
        return (*this);
    }
    // sets WiFi Credentials
    Span &setWifiCredentials(const char *ssid, const char *pwd);
    // sets an optional user-defined function to call when HomeSpan status changes
    Span &setStatusCallback(void (*f)(HS_STATUS status))
    {
        statusCallback = f;
        return (*this);
    }
    // returns char string for HomeSpan status change messages
    const char *statusString(HS_STATUS s);
    // sets the Pairing Code - use is NOT recommended.  Use 'S' from CLI instead
    Span &setPairingCode(const char *s, boolean progCall = true);
    // deletes stored Characteristic values from NVS
    void deleteStoredValues() { processSerialCommand("V"); }
    // resets the IID count for the current Accessory to start at newIID
    Span &resetIID(uint32_t newIID);
    // sets an optional user-defined function to call whenever a Controller is added/removed/changed
    Span &setControllerCallback(void (*f)())
    {
        controllerCallback = f;
        return (*this);
    }

    // sets the hostName suffix to be used instead of the 6-byte AccessoryID
    Span &setHostNameSuffix(const char *suffix)
    {
        asprintf(&hostNameSuffix, "%s", suffix);
        return (*this);
    }
    // enables Over-the-Air updates, with (auth=true) or without (auth=false) authorization password
    int enableOTA(boolean auth = true, boolean safeLoad = true) { return (spanOTA.init(auth, safeLoad, NULL)); }
    // enables Over-the-Air updates, with custom authorization password
    // (overrides any password stored with the 'O' command)
    int enableOTA(const char *pwd, boolean safeLoad = true) { return (spanOTA.init(true, safeLoad, pwd)); }

    // enable Web Logging
    Span &enableWebLog(uint16_t maxEntries = 0,
                       const char *serv = NULL,
                       const char *tz = "UTC",
                       const char *url = DEFAULT_WEBLOG_URL)
    {
        webLog.init(maxEntries, serv, tz, url);
        return (*this);
    }

    // add Web Log entry
    void addWebLog(boolean sysMsg, const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        webLog.vLog(sysMsg, fmt, ap);
        va_end(ap);
    }

    Span &setWebLogCSS(const char *css)
    {
        webLog.css = "\n" + String(css) + "\n";
        return (*this);
    }
    Span &setWebLogCallback(void (*f)(String &))
    {
        weblogCallback = f;
        return (*this);
    }
    void getWebLog(void (*f)(const char *, void *), void *);

    Span &setVerboseWifiReconnect(bool verbose = true)
    {
        verboseWifiReconnect = verbose;
        return (*this);
    }

    Span &setRebootCallback(void (*f)(uint8_t), uint32_t t = DEFAULT_REBOOT_CALLBACK_TIME)
    {
        rebootCallback = f;
        rebootCallbackTime = t;
        return (*this);
    }

    // start pollTask()
    void autoPoll(uint32_t stackSize = 8192, uint32_t priority = 1, uint32_t cpu = 0)
    {
        xTaskCreateUniversal(
            [](void *parms) {
                for (;;) {
                    homeSpan.pollTask();
                    vTaskDelay(5);
                }
            },
            "pollTask", stackSize, NULL, priority, &pollTaskHandle, cpu);
        LOG0("\n*** AutoPolling Task started with priority=%d\n\n", uxTaskPriorityGet(pollTaskHandle));
    }

    TaskHandle_t getAutoPollTask() { return (pollTaskHandle); }

    // sets wait time (in seconds) for optional web log time server to connect
    Span &setTimeServerTimeout(uint32_t tSec)
    {
        webLog.waitTime = tSec * 1000;
        return (*this);
    }

    list<Controller, Mallocator<Controller>>::const_iterator controllerListBegin();
    list<Controller, Mallocator<Controller>>::const_iterator controllerListEnd();

    [[deprecated(
        "This function has been deprecated (it is not needed) and no longer does anything.  Please remove from sketch "
        "to ensure backwards compatilibilty with future versions.")]]
    Span &reserveSocketConnections(uint8_t n)
    {
        return (*this);
    }
};

///////////////////////////////

class SpanAccessory
{
    friend class Span;
    friend class SpanService;
    friend class SpanCharacteristic;
    friend class SpanButton;

    // Accessory Instance ID (HAP Table 6-1)
    uint32_t aid = 0;
    // running count of iid to use for Services and Characteristics associated with this Accessory
    uint32_t iidCount = 0;
    // vector of pointers to all Services in this Accessory
    vector<SpanService *, Mallocator<SpanService *>> Services;

    // writes Accessory JSON to hapOut stream
    void printfAttributes(int flags);

  protected:
    // destructor
    ~SpanAccessory();

  public:
    // override new operator to use PSRAM when available
    void *operator new(size_t size) { return (HS_MALLOC(size)); }
    // constructor
    SpanAccessory(uint32_t aid = 0);
};

///////////////////////////////

class SpanService
{
    friend class Span;
    friend class SpanAccessory;
    friend class SpanCharacteristic;

    // Instance ID (HAP Table 6-2)
    uint32_t iid = 0;
    // Service Type
    const char *type;
    // HAP Name
    const char *hapName;
    // optional property indicating service is hidden
    boolean hidden = false;
    // optional property indicating service is primary
    boolean primary = false;
    // vector of pointers to all Characteristics in this Service
    vector<SpanCharacteristic *, Mallocator<SpanCharacteristic *>> Characteristics;
    // vector of pointers to any optional linked Services
    vector<SpanService *, Mallocator<SpanService *>> linkedServices;
    // flag to indicate this is a Custom Service
    boolean isCustom;
    // pointer to Accessory containing this Service
    SpanAccessory *accessory = NULL;

    // writes Service JSON to hapOut stream
    void printfAttributes(int flags);

  protected:
    // destructor
    virtual ~SpanService();
    // vector of pointers to all required HAP Characteristic Types for this Service
    vector<HapChar *, Mallocator<HapChar *>> req;
    // vector of pointers to all optional HAP Characteristic Types for this Service
    vector<HapChar *, Mallocator<HapChar *>> opt;

  public:
    // override new operator to use PSRAM when available
    void *operator new(size_t size) { return (HS_MALLOC(size)); }
    // constructor
    SpanService(const char *type, const char *hapName, boolean isCustom = false);
    // sets the Service Type to be primary and returns pointer to self
    SpanService *setPrimary();
    // sets the Service Type to be hidden and returns pointer to self
    SpanService *setHidden();
    // adds svc as a Linked Service and returns pointer to self
    SpanService *addLink(SpanService *svc);

    // returns linkedServices vector, mapped to <T>, for use as range in "for-each" loops
    template <typename T = SpanService *>
    vector<T, Mallocator<T>> getLinks(const char *hapName = NULL)
    {
        vector<T, Mallocator<T>> v;
        for (auto svc : linkedServices) {
            if (hapName == NULL || !strcmp(hapName, svc->hapName))
                v.push_back(static_cast<T>(svc));
        }
        return (v);
    }

    // returns IID of Service
    uint32_t getIID() { return (iid); }

    // placeholder for code that is called when a Service is updated via a Controller.  Must return true/false depending
    // on success of update
    virtual boolean update() { return (true); }
    // loops for each Service - called every cycle if over-ridden with user-defined code
    virtual void loop() {}
    // method called for a Service when a button attached to "pin" has a Single, Double, or Long Press, according to
    // pressType
    virtual void button(int pin, int pressType) {}
};

///////////////////////////////

class SpanCharacteristic
{
    friend class Span;
    friend class SpanService;

    union UVal
    {
        boolean BOOL;
        uint8_t UINT8;
        uint16_t UINT16;
        uint32_t UINT32;
        uint64_t UINT64;
        int32_t INT;
        double FLOAT;
        char *STRING = NULL;
    };

    // vector of current connections that have subscribed to EV notifications for this Characteristic
    class EVLIST : public vector<HAPClient *, Mallocator<HAPClient *>>
    {
      public:
        // returns true if pointer to connection hc is subscribed, else returns false
        boolean has(HAPClient *hc);
        // adds connection hc as new subscriber, IF not already a subscriber
        void add(HAPClient *hc);
        // removes connection hc as a subscriber; okay to remove even if hc was not already a subscriber
        void remove(HAPClient *hc);
    };

    uint32_t iid = 0;             // Instance ID (HAP Table 6-3)
    HapChar *hapChar;             // pointer to HAP Characteristic structure
    const char *type;             // Characteristic Type
    const char *hapName;          // HAP Name
    UVal value;                   // Characteristic Value
    uint8_t perms;                // Characteristic Permissions
    FORMAT format;                // Characteristic Format
    char *desc = NULL;            // Characteristic Description (optional)
    char *unit = NULL;            // Characteristic Unit (optional)
    UVal minValue;                // Characteristic minimum (not applicable for STRING)
    UVal maxValue;                // Characteristic maximum (not applicable for STRING)
    UVal stepValue;               // Characteristic step size (not applicable for STRING)
    boolean staticRange;          // Flag that indicates whether Range is static and cannot be changed with setRange()
    boolean customRange = false;  // Flag for custom ranges
    char *validValues = NULL;     // Optional JSON array of valid values.  Applicable only to uint8 Characteristics
    char *nvsKey = NULL;          // key for NVS storage of Characteristic value
    boolean isCustom;             // flag to indicate this is a Custom Characteristic
    boolean setRangeError =
        false;  // flag to indicate attempt to set Range on Characteristic that does not support changes to Range
    boolean setValidValuesError = false;  // flag to indicate attempt to set Valid Values on Characteristic that does
                                          // not support changes to Valid Values

    uint32_t aid = 0;        // Accessory ID - passed through from Service containing this Characteristic
    uint8_t updateFlag = 0;  // set to either 1 (for normal write) or 2 (for write-response) inside update() when
                             // Characteristic is successfully updated via Home App
    unsigned long updateTime =
        0;          // last time value was updated (in millis) either by PUT /characteristic OR by setVal()
    UVal newValue;  // the updated value requested by PUT /characteristic
    SpanService *service = NULL;  // pointer to Service containing this Characteristic
    EVLIST evList;  // vector of current connections that have subscribed to EV notifications for this Characteristic

    // writes Characteristic JSON to hapOut stream
    void printfAttributes(int flags);
    // load updated val/ev from PUT /characteristic JSON request.  Return intitial
    // HAP status code (checks to see if characteristic is found, is writable, etc.)
    StatusCode loadUpdate(char *val, char *ev, boolean wr);
    // returns "printable" String for any type of Characteristic
    String uvPrint(UVal &u);

    void uvSet(UVal &dest, UVal &src);   // copies UVal src into UVal dest
    void uvSet(UVal &u, STRING_t val);   // copies string val into UVal u
    void uvSet(UVal &u, DATA_t data);    // copies DATA data into UVal u (after transforming to a char *)
    void uvSet(UVal &u, TLV_ENC_t tlv);  // copies TLV8 tlv into UVal u (after transforming to a char *)

    // copies numeric val into UVal u
    template <typename T>
    void uvSet(UVal &u, T val)
    {
        switch (format) {
            case FORMAT::BOOL:
                u.BOOL = (boolean)val;
                break;
            case FORMAT::INT:
                u.INT = (int)val;
                break;
            case FORMAT::UINT8:
                u.UINT8 = (uint8_t)val;
                break;
            case FORMAT::UINT16:
                u.UINT16 = (uint16_t)val;
                break;
            case FORMAT::UINT32:
                u.UINT32 = (uint32_t)val;
                break;
            case FORMAT::UINT64:
                u.UINT64 = (uint64_t)val;
                break;
            case FORMAT::FLOAT:
                u.FLOAT = (double)val;
                break;
            default:
                break;
        }  // switch
    }

    // gets the specified UVal for string-based Characteristics
    char *getStringGeneric(UVal &val);
    // gets the specified UVal for data-based Characteristics
    size_t getDataGeneric(uint8_t *data, size_t len, UVal &val);
    // gets the specified UVal for tlv8-based Characteristics
    size_t getTLVGeneric(TLV8 &tlv, UVal &val);

    // gets the specified UVal for numeric-based Characteristics
    template <class T>
    T uvGet(UVal &u)
    {
        switch (format) {
            case FORMAT::BOOL:
                return ((T)u.BOOL);
            case FORMAT::INT:
                return ((T)u.INT);
            case FORMAT::UINT8:
                return ((T)u.UINT8);
            case FORMAT::UINT16:
                return ((T)u.UINT16);
            case FORMAT::UINT32:
                return ((T)u.UINT32);
            case FORMAT::UINT64:
                return ((T)u.UINT64);
            case FORMAT::FLOAT:
                return ((T)u.FLOAT);
            default:
                break;
        }
        return ((T)0);  // included to prevent compiler warnings
    }

    void setValCheck();                 // initial check before setting value of any Characteristic
    void setValFinish(boolean notify);  // final processing after setting value of any Characteristic

  protected:
    ~SpanCharacteristic();  // destructor

    template <typename T>
    void init(T val, boolean nvsStore, T min, T max)
    {
        uvSet(value, val);

        if (nvsStore) {
            nvsKey = (char *)HS_MALLOC(16);
            uint16_t t;
            sscanf(type, "%hx", &t);
            sprintf(nvsKey, "%04X%08X%03X", t, aid, iid & 0xFFF);
            size_t len;

            if (format < FORMAT::STRING) {
                if (nvs_get_u64(homeSpan.charNVS, nvsKey, &(value.UINT64)) != ESP_OK) {
                    // store data as uint64_t regardless of actual type (it will be read correctly when access through
                    // uvGet())
                    nvs_set_u64(homeSpan.charNVS, nvsKey, value.UINT64);
                    // commit to NVS
                    nvs_commit(homeSpan.charNVS);
                }
            } else {
                if (!nvs_get_str(homeSpan.charNVS, nvsKey, NULL, &len)) {
                    value.STRING = (char *)HS_REALLOC(value.STRING, len);
                    nvs_get_str(homeSpan.charNVS, nvsKey, value.STRING, &len);
                } else {
                    nvs_set_str(homeSpan.charNVS, nvsKey, value.STRING);  // store string data
                    nvs_commit(homeSpan.charNVS);                         // commit to NVS
                }
            }
        }

        uvSet(newValue, value);

        if (format < FORMAT::STRING) {
            uvSet(minValue, min);
            uvSet(maxValue, max);
            uvSet(stepValue, 0);
        }
    }

  public:
    // SpanCharacteristic constructor
    SpanCharacteristic(HapChar *hapChar, boolean isCustom = false);
    // override new operator to use PSRAM when available
    void *operator new(size_t size) { return (HS_MALLOC(size)); }

    // gets the value for numeric-based Characteristics
    template <class T = int>
    T getVal()
    {
        return (uvGet<T>(value));
    }
    // gets the value for string-based Characteristics
    char *getString() { return (getStringGeneric(value)); }
    // gets the value for data-based Characteristics
    size_t getData(uint8_t *data, size_t len) { return (getDataGeneric(data, len, value)); }
    // gets the value for tlv8-based Characteristics
    size_t getTLV(TLV8 &tlv) { return (getTLVGeneric(tlv, value)); }

    // gets the newValue for numeric-based Characteristics
    template <class T = int>
    T getNewVal()
    {
        return (uvGet<T>(newValue));
    }
    // gets the newValue for string-based Characteristics
    char *getNewString() { return (getStringGeneric(newValue)); }
    // gets the newValue for data-based Characteristics
    size_t getNewData(uint8_t *data, size_t len) { return (getDataGeneric(data, len, newValue)); }
    // gets the newValue for tlv8-based Characteristics
    size_t getNewTLV(TLV8 &tlv) { return (getTLVGeneric(tlv, newValue)); }

    // sets the value and newValue for string-based Characteristic
    void setString(const char *val, boolean notify = true);
    // sets the value and newValue for data-based Characteristic
    void setData(const uint8_t *data, size_t len, boolean notify = true);
    // sets the value and newValue for tlv8-based Characteristic
    void setTLV(const TLV8 &tlv, boolean notify = true);

    // sets the value and newValue for numeric-based Characteristics
    template <typename T>
    void setVal(T val, boolean notify = true)
    {
        setValCheck();

        if (!((val >= uvGet<T>(minValue)) && (val <= uvGet<T>(maxValue)))) {
            LOG0(
                "\n*** WARNING:  Attempt to update Characteristic::%s with setVal(%g) is out of range [%g,%g].  This "
                "may cause device to become non-responsive!\n\n",
                hapName, (double)val, uvGet<double>(minValue), uvGet<double>(maxValue));
        }

        uvSet(value, val);
        uvSet(newValue, value);

        updateTime = homeSpan.snapTime;

        if (notify) {
            if (updateFlag != 2) {           // do not broadcast EV if update is being done in context of write-response
                SpanBuf sb;                  // create SpanBuf object
                sb.characteristic = this;    // set characteristic
                sb.status = StatusCode::OK;  // set status
                char dummy[] = "";
                sb.val = dummy;  // set dummy "val" so that printfNotify knows to consider this "update"
                homeSpan.Notifications.push_back(sb);  // store SpanBuf in Notifications vector
            }

            if (nvsKey) {
                nvs_set_u64(homeSpan.charNVS, nvsKey,
                            value.UINT64);  // store data as uint64_t regardless of actual type (it will be read
                                            // correctly when access through uvGet())
                nvs_commit(homeSpan.charNVS);
            }
        }
    }

    // returns true within update() if Characteristic was updated by Home App
    boolean updated();
    // returns time elapsed (in millis) since value was last updated, either by Home App or by using setVal()
    unsigned long timeVal();

    // returns IID of Characteristic
    uint32_t getIID();

    // sets permissions of a Characteristic
    SpanCharacteristic *setPerms(uint8_t perms);
    // add permissions of a Characteristic
    SpanCharacteristic *addPerms(uint8_t dPerms);
    // removes permissions of a Characteristic
    SpanCharacteristic *removePerms(uint8_t dPerms);
    // sets description of a Characteristic
    SpanCharacteristic *setDescription(const char *c);
    // set unit of a Characteristic
    SpanCharacteristic *setUnit(const char *c);
    // sets a list of 'n' valid values allowed for a Characteristic - only applicable if format=INT, UINT8, UINT16, or
    // UINT32
    SpanCharacteristic *setValidValues(int n, ...);

    // sets the allowed range of a Characteristic
    template <typename A, typename B, typename S = int>
    SpanCharacteristic *setRange(A min, B max, S step = 0)
    {
        if (!staticRange) {
            uvSet(minValue, min);
            uvSet(maxValue, max);
            uvSet(stepValue, step);
            customRange = true;
        } else
            setRangeError = true;

        return (this);

    }  // setRange()
};

///////////////////////////////

class SpanButton : public PushButton
{
    friend class Span;
    friend class SpanService;

    uint16_t singleTime;   // minimum time (in millis) required to register a single press
    uint16_t longTime;     // minimum time (in millis) required to register a long press
    uint16_t doubleTime;   // maximum time (in millis) between single presses to register a double press instead
    SpanService *service;  // Service to which this PushButton is attached

    // check PushButton and call button() if "pressed"
    void check();

  protected:
    enum buttonType_t
    {
        HS_BUTTON,
        HS_TOGGLE
    };

    // type of SpanButton
    buttonType_t buttonType = HS_BUTTON;

  public:
    SpanButton(int pin,
               uint16_t longTime = 2000,
               uint16_t singleTime = 5,
               uint16_t doubleTime = 200,
               triggerType_t triggerType = TRIGGER_ON_LOW);
    SpanButton(int pin,
               triggerType_t triggerType,
               uint16_t longTime = 2000,
               uint16_t singleTime = 5,
               uint16_t doubleTime = 200)
        : SpanButton(pin, longTime, singleTime, doubleTime, triggerType) {};
};

///////////////////////////////

class SpanToggle : public SpanButton
{
  public:
    SpanToggle(int pin, triggerType_t triggerType = TRIGGER_ON_LOW, uint16_t toggleTime = 5)
        : SpanButton(pin, triggerType, toggleTime)
    {
        buttonType = HS_TOGGLE;
    };
    int position() { return (pressType); }
};

///////////////////////////////

class SpanUserCommand
{
    friend class Span;

    const char *s;                                           // description of command
    void (*userFunction1)(const char *v) = NULL;             // user-defined function to call
    void (*userFunction2)(const char *v, void *arg) = NULL;  // user-defined function to call with user-defined arg
    void *userArg;

  public:
    SpanUserCommand(char c, const char *s, void (*f)(const char *));
    SpanUserCommand(char c, const char *s, void (*f)(const char *, void *), void *arg);
};

///////////////////////////////

class SpanPoint
{
    friend class Span;

    int receiveSize;               // size (in bytes) of messages to receive
    int sendSize;                  // size (in bytes) of messages to send
    esp_now_peer_info_t peerInfo;  // structure for all ESP-NOW peer data
    QueueHandle_t receiveQueue;    // queue to store data after it is received
    uint32_t receiveTime = 0;      // time (in millis) of most recent data received

    static uint8_t lmk[16];
    static boolean initialized;
    static boolean isHub;
    static boolean useEncryption;
    static vector<SpanPoint *, Mallocator<SpanPoint *>> SpanPoints;
    static uint16_t channelMask;       // channel mask (only used for remote devices)
    static QueueHandle_t statusQueue;  // queue for communication between SpanPoint::dataSend and SpanPoint::send
    static nvs_handle pointNVS;        // NVS storage for channel number (only used for remote devices)

    static void dataReceived(const uint8_t *mac, const uint8_t *incomingData, int len);
    static void init(const char *password = "HomeSpan");
    static void setAsHub() { isHub = true; }
    static uint8_t nextChannel();

    static void dataSent(const uint8_t *mac, esp_now_send_status_t status) { xQueueOverwrite(statusQueue, &status); }

  public:
    SpanPoint(const char *macAddress, int sendSize, int receiveSize, int queueDepth = 1, boolean useAPaddress = false);
    static void setPassword(const char *pwd) { init(pwd); }
    static void setChannelMask(uint16_t mask);
    static void setEncryption(boolean encrypt) { useEncryption = encrypt; }
    boolean get(void *dataBuf);
    boolean send(const void *data);
    uint32_t time() { return (millis() - receiveTime); }
};

/////////////////////////////////////////////////

#include "Span.h"
