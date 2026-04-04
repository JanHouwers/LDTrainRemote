#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Bounce2.h>
#include <esp_sleep.h>
#include "../config.h"

// ── LEGO Wireless Protocol ──────────────────────────────
static NimBLEUUID kServiceUuid("00001623-1212-efde-1623-785feabcd123");
static NimBLEUUID kCharUuid("00001624-1212-efde-1623-785feabcd123");
static const uint8_t kLegoMfgPrefix[] = {0x97, 0x03};

// ── Raw LWP3 Packets ────────────────────────────────────
// Motor: byte[7]=command (0x01=fwd, 0x02=bck, 0x03=stop), byte[9]=speed
static uint8_t kMotorPacket[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x01, 0x01, 0x00, 0x00};
static const uint8_t kStopPacket[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x03, 0x01, 0x00, 0x00};
static const uint8_t kHornPacket[] = {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x07, 0x01, 0x00, 0x00};

// LED color packets: byte[9] = color code
static const uint8_t kLedPackets[][11] = {
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x00, 0x00}, // Off
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x01, 0x00}, // White
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x07, 0x00}, // Green
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x08, 0x00}, // Yellow
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x09, 0x00}, // Light Blue
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0F, 0x00}, // Dark Blue
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0A, 0x00}, // Purple
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0E, 0x00}, // Purple Pink
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0B, 0x00}, // Light Pink
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0D, 0x00}, // Red Pink
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x04, 0x01, 0x0C, 0x00}, // Red
};
static const size_t kNumLedColors = sizeof(kLedPackets) / sizeof(kLedPackets[0]);

// Action/water packets: byte[9] = action code
static const uint8_t kActionPackets[][11] = {
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x06, 0x00}, // Recorded sound
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x02, 0x00}, // Cat
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x00, 0x00}, // Nothing
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x01, 0x00}, // Beach
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x05, 0x00}, // Rain
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x04, 0x00}, // Birthday
    {0x0B, 0x00, 0x81, 0x34, 0x11, 0x51, 0x01, 0x06, 0x01, 0x03, 0x00}, // Night
};
static const size_t kNumActions = sizeof(kActionPackets) / sizeof(kActionPackets[0]);

// ── BLE State ────────────────────────────────────────────
static NimBLEAddress g_targetAddr;
static bool g_haveTarget = false;
static NimBLEClient *g_client = nullptr;
static NimBLERemoteCharacteristic *g_char = nullptr;
static bool g_scanRunning = false;
static unsigned long g_lastScanLogMs = 0;
static uint32_t g_advSeen = 0;

// ── UI State ─────────────────────────────────────────────
static Bounce pbStop = Bounce();
static Bounce pbHorn = Bounce();
static Bounce pbLed = Bounce();
static Bounce pbWater = Bounce();

static size_t g_ledIndex = 0;
static size_t g_actionIndex = 0;
static int g_lastSpeed = 0; // last sent speed: positive=fwd, negative=bck, 0=stop

// ── Inactivity / Deep Sleep ─────────────────────────────
static unsigned long g_lastActivityMs = 0;
static int g_lastPotRaw = 0;

static void resetActivityTimer() {
    g_lastActivityMs = millis();
}

static void enterDeepSleep() {
    Serial.println("Inactivity timeout — entering deep sleep");
    Serial.flush();

    // Disconnect BLE cleanly
    if (g_client && g_client->isConnected()) {
        g_client->disconnect();
    }
    cleanupClient();
    NimBLEDevice::getScan()->stop();
    NimBLEDevice::deinit(true);

    // Wake on any button press (active-low)
    const uint64_t wakeupMask = (1ULL << BTN_STOP_PIN)
                              | (1ULL << BTN_HORN_PIN)
                              | (1ULL << BTN_LED_PIN)
                              | (1ULL << BTN_WATER_PIN);
    esp_deep_sleep_enable_gpio_wakeup(wakeupMask, ESP_GPIO_WAKEUP_GPIO_LOW);

    esp_deep_sleep_start();
}

// ── BLE Helpers ──────────────────────────────────────────
static bool isLegoTrain(const NimBLEAdvertisedDevice &adv) {
    if (!adv.haveManufacturerData()) return false;
    std::string m = adv.getManufacturerData();
    if (m.size() < 2) return false;
    return memcmp(m.data(), kLegoMfgPrefix, 2) == 0;
}

static void cleanupClient() {
    if (g_client) {
        NimBLEDevice::deleteClient(g_client);
        g_client = nullptr;
    }
    g_char = nullptr;
    g_haveTarget = false;
    g_lastSpeed = 0;
}

static void startScan() {
    if (g_scanRunning) return;
    NimBLEDevice::deleteAllBonds();
    g_advSeen = 0;
    NimBLEDevice::getScan()->start(0, false, false);
    g_scanRunning = true;
    Serial.println("BLE scan started");
}

class AdvCallbacks : public NimBLEScanCallbacks {
    void handleResult(const NimBLEAdvertisedDevice *adv) {
        g_advSeen++;
        if (g_advSeen <= 5 || (g_advSeen % 50 == 0)) {
            Serial.printf("Adv #%u from %s RSSI=%d\n", g_advSeen,
                          adv->getAddress().toString().c_str(), adv->getRSSI());
        }
        if (isLegoTrain(*adv)) {
            Serial.printf("Found Duplo Train at %s RSSI=%d\n",
                          adv->getAddress().toString().c_str(), adv->getRSSI());
            g_targetAddr = adv->getAddress();
            g_haveTarget = true;
            NimBLEDevice::getScan()->stop();
            g_scanRunning = false;
        }
    }
    void onResult(const NimBLEAdvertisedDevice *adv) override { handleResult(adv); }
    void onResult(NimBLEAdvertisedDevice *adv) { handleResult(adv); }
    void onScanEnd(NimBLEScanResults results) { g_scanRunning = false; }
    void onScanEnd(const NimBLEScanResults *results) { g_scanRunning = false; }
};

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *c) override {
        Serial.println("Connected");
        c->updateConnParams(24, 24, 0, 120);
    }
    void onDisconnect(NimBLEClient *c, int reason) override {
        Serial.printf("Disconnected (reason=%d)\n", reason);
        // Don't cleanup or scan from callback context; let loop() handle it
        g_char = nullptr;
    }
    bool onConnParamsUpdateRequest(NimBLEClient *c, const ble_gap_upd_params *params) override {
        return true;
    }
};

static ClientCallbacks g_clientCb;

static bool connectAndDiscover() {
    if (!g_haveTarget) return false;
    Serial.printf("Connecting to %s\n", g_targetAddr.toString().c_str());

    cleanupClient();
    g_haveTarget = true; // restore — cleanupClient clears it

    NimBLEDevice::deleteAllBonds();
    g_client = NimBLEDevice::createClient();
    g_client->setClientCallbacks(&g_clientCb, false);

    if (!g_client->connect(g_targetAddr)) {
        Serial.println("Connect failed");
        cleanupClient();
        delay(1000);
        startScan();
        return false;
    }

    NimBLERemoteService *svc = g_client->getService(kServiceUuid);
    if (!svc) {
        Serial.println("Service not found");
        g_client->disconnect();
        cleanupClient();
        startScan();
        return false;
    }

    g_char = svc->getCharacteristic(kCharUuid);
    if (!g_char) {
        Serial.println("Characteristic not found");
        g_client->disconnect();
        cleanupClient();
        startScan();
        return false;
    }

    if (g_char->canNotify()) {
        g_char->subscribe(true, [](NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
            Serial.print("Notify: ");
            for (size_t i = 0; i < len; ++i) Serial.printf("%02X ", data[i]);
            Serial.println();
        });
    }

    Serial.println("Connected and ready");
    return true;
}

// ── Motor Control ────────────────────────────────────────
static const int SPEED_THRESHOLD = 3;            // ignore ADC jitter smaller than this
static const unsigned long MOTOR_MIN_MS = 200;   // minimum interval between motor writes
static unsigned long g_lastMotorMs = 0;

static void sendMotorSpeed(int speed) {
    if (!g_char) return;
    if (speed == 0) {
        g_char->writeValue(kStopPacket, sizeof(kStopPacket), false);
    } else {
        uint8_t absSpeed = constrain(abs(speed), 0, SPEED_MAX);
        kMotorPacket[7] = (speed > 0) ? 0x01 : 0x02; // fwd or bck
        kMotorPacket[9] = absSpeed;
        g_char->writeValue(kMotorPacket, sizeof(kMotorPacket), false);
    }
}

// ── Potentiometer ────────────────────────────────────────
static void handlePoti() {
    int raw = analogRead(POT_PIN);
    int speed = 0;
    int lo = POT_MID - POT_DEADBAND; // 1741
    int hi = POT_MID + POT_DEADBAND; // 2355

    if (raw < lo) {
        speed = map(raw, POT_MIN, lo, SPEED_MAX, 0);
    } else if (raw > hi) {
        speed = map(raw, hi, POT_MAX, 0, -SPEED_MAX);
    }

    // Detect potentiometer movement as user activity
    if (abs(raw - g_lastPotRaw) > POT_ACTIVITY_THRESHOLD) {
        g_lastPotRaw = raw;
        resetActivityTimer();
    }

    int diff = abs(speed - g_lastSpeed);
    // Always send on stop/start transitions; otherwise require threshold + minimum interval
    bool isTransition = (speed == 0) != (g_lastSpeed == 0);
    if (isTransition || (diff >= SPEED_THRESHOLD && millis() - g_lastMotorMs >= MOTOR_MIN_MS)) {
        sendMotorSpeed(speed);
        g_lastSpeed = speed;
        g_lastMotorMs = millis();
    }
}

// ── Buttons ──────────────────────────────────────────────
static void handleButtons() {
    bool anyPressed = false;
    if (pbStop.update() && pbStop.fell()) {
        g_char->writeValue(kStopPacket, sizeof(kStopPacket), true);
        g_lastSpeed = 0;
        Serial.println("Button: Stop");
        anyPressed = true;
    }
    if (pbHorn.update() && pbHorn.fell()) {
        g_char->writeValue(kHornPacket, sizeof(kHornPacket), true);
        Serial.println("Button: Horn");
        anyPressed = true;
    }
    if (pbLed.update() && pbLed.fell()) {
        g_ledIndex = (g_ledIndex + 1) % kNumLedColors;
        g_char->writeValue(kLedPackets[g_ledIndex], sizeof(kLedPackets[0]), true);
        Serial.printf("Button: LED color %u\n", (unsigned)g_ledIndex);
        anyPressed = true;
    }
    if (pbWater.update() && pbWater.fell()) {
        g_actionIndex = (g_actionIndex + 1) % kNumActions;
        g_char->writeValue(kActionPackets[g_actionIndex], sizeof(kActionPackets[0]), true);
        Serial.printf("Button: Action %u\n", (unsigned)g_actionIndex);
        anyPressed = true;
    }
    if (anyPressed) resetActivityTimer();
}

// ── Setup & Loop ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("Duplo Train Remote (raw NimBLE)");

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("Woke up from deep sleep (button press)");
    }

    // Initialize activity timer and potentiometer baseline
    g_lastActivityMs = millis();
    g_lastPotRaw = analogRead(POT_PIN);

    // Buttons with Bounce2 debouncing
    pbStop.attach(BTN_STOP_PIN, INPUT_PULLUP);
    pbHorn.attach(BTN_HORN_PIN, INPUT_PULLUP);
    pbLed.attach(BTN_LED_PIN, INPUT_PULLUP);
    pbWater.attach(BTN_WATER_PIN, INPUT_PULLUP);
    pbStop.interval(DEBOUNCE_MS);
    pbHorn.interval(DEBOUNCE_MS);
    pbLed.interval(DEBOUNCE_MS);
    pbWater.interval(DEBOUNCE_MS);

    // BLE init
    NimBLEDevice::init("");
    NimBLEDevice::deleteAllBonds();
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(new AdvCallbacks(), true);
    scan->setInterval(45);
    scan->setWindow(30);
    scan->setActiveScan(true);

    Serial.println("Starting BLE scan...");
    bool ok = scan->start(0, false, false);
    Serial.printf("Scan start %s\n", ok ? "ok" : "FAILED");
    g_scanRunning = ok;
}

void loop() {
    // Check inactivity timeout — sleep if idle too long
    if (millis() - g_lastActivityMs >= SLEEP_TIMEOUT_MS) {
        enterDeepSleep();
    }

    // Detect disconnection: client exists but lost connection
    if (g_client && !g_client->isConnected()) {
        Serial.println("Connection lost; cleaning up");
        cleanupClient();
        delay(1000);
        startScan();
    }

    // Not connected: scan and connect
    if (!g_client) {
        if (g_haveTarget) {
            connectAndDiscover();
        } else if (!g_scanRunning) {
            startScan();
        } else {
            unsigned long now = millis();
            if (now - g_lastScanLogMs > 3000) {
                Serial.printf("Scanning... advSeen=%u\n", g_advSeen);
                if (!NimBLEDevice::getScan()->isScanning()) {
                    Serial.println("Scan stopped unexpectedly; restarting");
                    g_scanRunning = false;
                    startScan();
                }
                g_lastScanLogMs = now;
            }
        }
        delay(50);
        return;
    }

    // Connected: handle inputs
    handleButtons();
    handlePoti();
    delay(LOOP_DELAY_MS);
}
