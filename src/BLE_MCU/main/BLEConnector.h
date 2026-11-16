#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static BLEUUID clientServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static BLEUUID clientCharUUID("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

const char* SENSOR_C4 = "C4642CE7F28C2DD9_ACC";
const char* SENSOR_E6 = "E6392E1C62A7F1CC_ACC";

struct Sample { uint32_t time_ms; float x, y, z; };

typedef void (*NotifyCallBack)(BLERemoteCharacteristic*, uint8_t*, size_t, bool);

typedef struct {
    const char*          name;
    BLEAdvertisedDevice* advertisedDevice;
    BLEClient*           client;
    bool                 doConnect;
    NotifyCallBack       callback;
} SensorCfg;

SemaphoreHandle_t bleMutex;
BLEScan* pBLEScan = nullptr;
SensorCfg c4, e6;

static void notifyCallback1(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
    if (length % sizeof(Sample) != 0) return;
    Sample* samples = (Sample*)pData;
    for (size_t i = 0; i < length / sizeof(Sample); i++)
        Serial.printf("[%s] [%u ms] x=%.3f y=%.3f z=%.3f\n",
            SENSOR_C4, samples[i].time_ms, samples[i].x, samples[i].y, samples[i].z);
}

static void notifyCallback2(BLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
    if (length % sizeof(Sample) != 0) return;
    Sample* samples = (Sample*)pData;
    for (size_t i = 0; i < length / sizeof(Sample); i++)
        Serial.printf("[%s] [%u ms] x=%.3f y=%.3f z=%.3f\n",
            SENSOR_E6, samples[i].time_ms, samples[i].x, samples[i].y, samples[i].z);
}

class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (!advertisedDevice.haveName()) return;
        String name = advertisedDevice.getName().c_str();

        if (name == SENSOR_C4 && c4.advertisedDevice == nullptr) {
            c4.advertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
            c4.doConnect = true;
            Serial.println("Found SENSOR_C4");
        }
        if (name == SENSOR_E6 && e6.advertisedDevice == nullptr) {
            e6.advertisedDevice = new BLEAdvertisedDevice(advertisedDevice);
            e6.doConnect = true;
            Serial.println("Found SENSOR_E6");
        }
        if (c4.advertisedDevice && e6.advertisedDevice) {
            BLEDevice::getScan()->stop();
            BLEDevice::getScan()->clearResults();
        }
    }
};

bool connectToServer(SensorCfg* cfg) {
    if (!cfg->client)
        cfg->client = BLEDevice::createClient();

    if (!cfg->client->connect(cfg->advertisedDevice)) {
        Serial.printf("Failed to connect to %s\n", cfg->name);
        return false;
    }
    cfg->client->setMTU(517);

    BLERemoteService* svc = cfg->client->getService(clientServiceUUID);
    if (!svc) { cfg->client->disconnect(); return false; }

    BLERemoteCharacteristic* rc = svc->getCharacteristic(clientCharUUID);
    if (!rc)  { cfg->client->disconnect(); return false; }

    if (rc->canRead())
        Serial.printf("[%s] Initial value: %s\n", cfg->name, rc->readValue().c_str());
    if (rc->canNotify()) {
        rc->registerForNotify(cfg->callback);
        Serial.printf("[%s] Registered for notifications\n", cfg->name);
    }
    return true;
}

void initBLE() {
    BLEDevice::init("ESP32-DualClient");
    bleMutex = xSemaphoreCreateMutex();

    c4.name = SENSOR_C4; c4.advertisedDevice = nullptr; c4.client = nullptr;
    c4.doConnect = false; c4.callback = notifyCallback1;

    e6.name = SENSOR_E6; e6.advertisedDevice = nullptr; e6.client = nullptr;
    e6.doConnect = false; e6.callback = notifyCallback2;

    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->start(10, true);
}