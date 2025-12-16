/**
 * @file hid_BLE.cpp
 *
 * @author Ángel Fernández Pineda. Madrid. Spain.
 * @date 2025-12-16
 * @brief Implementation of the hid namespace via a custom NimBLE wrapper
 *
 * @copyright Licensed under the EUPL
 *
 */

#include <cstring>

#include "SimWheel.hpp"
#include "SimWheelInternals.hpp"
#include "InternalServices.hpp"
#include "NimBLEWrapper.hpp"
// #include <Arduino.h> // For debugging

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

// Related to auto power off
static esp_timer_handle_t autoPowerOffTimer = nullptr;
static bool notifyConfigChanges = false;
static uint8_t inputReportData[GAMEPAD_REPORT_SIZE] = {0};

// ----------------------------------------------------------------------------
// BLE Server callbacks and advertising
// ----------------------------------------------------------------------------

void onConnectionStatus(bool connected)
{
    //************************************************
    // Do not call internals::hid::reset() here
    //************************************************
    // Quoting h2zero:
    //
    // When Windows bonds with a device and subscribes to
    // notifications/indications
    // of the device characteristics it does not re-subscribe
    // on subsequent connections.
    // If a notification is sent when Windows reconnects
    // it will overwrite the stored subscription value
    // in the NimBLE stack configuration with an invalid value which
    // results in notifications/indications not being sent.

    if (connected)
    {
        if (autoPowerOffTimer != nullptr)
            esp_timer_stop(autoPowerOffTimer);
    }
    else // disconnected
    {
        BLEAdvertising::start();
        OnDisconnected::notify();
        if (autoPowerOffTimer != nullptr)
            esp_timer_start_once(
                autoPowerOffTimer,
                AUTO_POWER_OFF_DELAY_SECS * 1000000);
    }
}

void startBLEAdvertising()
{
    onConnectionStatus(false);
}

// ----------------------------------------------------------------------------
// HID input report callback
// ----------------------------------------------------------------------------

uint16_t onInputReport(uint8_t reportId, uint8_t *data, uint16_t size)
{
    memcpy(data, inputReportData, size);
    return size;
}

// ----------------------------------------------------------------------------
// Auto power-off
// ----------------------------------------------------------------------------

void autoPowerOffCallback(void *unused)
{
    PowerService::call::shutdown();
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void internals::hid::begin(
    std::string deviceName,
    std::string deviceManufacturer,
    bool enableAutoPowerOff,
    uint16_t vendorID,
    uint16_t productID)
{
    if (!BLEDevice::initialized)
    {
        // Auto-power-off initialization
        if (enableAutoPowerOff)
        {
            esp_timer_create_args_t args;
            args.callback = &autoPowerOffCallback;
            args.arg = nullptr;
            args.name = nullptr;
            args.dispatch_method = ESP_TIMER_TASK;
            ESP_ERROR_CHECK(esp_timer_create(&args, &autoPowerOffTimer));
        }

        // Stack initialization
        BLEAdvertising::onConnectionStatus = onConnectionStatus;
        BLEHIDService::onReadInputReport = onInputReport;
        BLEHIDService::onGetFeatureReport =
            internals::hid::common::onGetFeature;
        BLEHIDService::onSetFeatureReport =
            internals::hid::common::onSetFeature;
        BLEHIDService::onWriteOutputReport =
            internals::hid::common::onOutput;
        BLEDeviceInfoService::manufacturer.name = deviceManufacturer;
        BLEDeviceInfoService::pnpInfo.vid = vendorID;
        BLEDeviceInfoService::pnpInfo.pid = productID;
        BLEDevice::init(deviceName);

        // Advertise
        startBLEAdvertising();
    }
}

// ----------------------------------------------------------------------------
// HID profile
// ----------------------------------------------------------------------------

void internals::hid::reset()
{
    internals::hid::common::onReset(inputReportData);
    // if (BLEAdvertising::connected())
    BLEHIDService::input_report.notify();
}

void internals::hid::reportInput(
    uint64_t inputsLow,
    uint64_t inputsHigh,
    uint8_t POVstate,
    uint8_t leftAxis,
    uint8_t rightAxis,
    uint8_t clutchAxis)
{
    internals::hid::common::onReportInput(
        inputReportData,
        notifyConfigChanges,
        inputsLow,
        inputsHigh,
        POVstate,
        leftAxis,
        rightAxis,
        clutchAxis);
    if (BLEAdvertising::connected())
        BLEHIDService::input_report.notify();
}

void internals::hid::reportBatteryLevel(int level)
{
    // if (BLEAdvertising::connected())
    //     {
    if (level > 100)
        level = 100;
    else if (level < 0)
        level = 0;
    BLEBatteryService::batteryLevel.set(level);
    // }
}

void internals::hid::reportChangeInConfig()
{
    notifyConfigChanges = true; // Will be reported in the next input report
}

bool internals::hid::supportsCustomHardwareID() { return true; }

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

bool internals::hid::isConnected()
{
    return BLEAdvertising::connected();
}
