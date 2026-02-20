/**
 * @file hid_USB_BLE.cpp
 *
 * @author Ángel Fernández Pineda. Madrid. Spain.
 * @date 2026-02-19
 * @brief Implementation of a HID device through the tinyUSB and
 *        NimBLE stacks
 *
 * @copyright Licensed under the EUPL
 *
 */

#include "USB.h"
#include "USBHID.h"
#include "NimBLEWrapper.hpp"

#include "SimWheel.hpp"
#include "SimWheelInternals.hpp"
#include "InternalServices.hpp"
#include "HID_definitions.hpp"
#include "esp_mac.h"       // For esp_efuse_mac_get_default()
#include "esp32-hal-log.h" // Logging
#include <cstring>         // For memcpy

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

static uint8_t inputReportData[GAMEPAD_REPORT_SIZE] = {0};
static bool notifyConfigChanges = false;
USBHID usbHidDevice;

// We create a new USB instance as the default stack size is not enough
ESPUSB USB_instance(4096);
#define USB USB_instance

void startBLEAdvertising(); // Forward declaration

// ----------------------------------------------------------------------------
// USB
// ----------------------------------------------------------------------------

class SimWheelHIDImpl : public USBHIDDevice
{
    virtual uint16_t _onGetDescriptor(uint8_t *buffer) override
    {
        memcpy(buffer, hid_descriptor, sizeof(hid_descriptor));
        return sizeof(hid_descriptor);
    }

    virtual uint16_t _onGetFeature(
        uint8_t report_id,
        uint8_t *buffer,
        uint16_t len) override
    {
        return internals::hid::common::onGetFeature(report_id, buffer, len);
    }

    virtual void _onSetFeature(
        uint8_t report_id,
        const uint8_t *buffer,
        uint16_t len) override
    {
        // Note: for unknown reasons, output reports trigger this callback
        // instead of _onOutput()
        if (report_id >= RID_OUTPUT_POWERTRAIN)
            // Output report
            internals::hid::common::onOutput(report_id, buffer, len);
        else
            // Feature report
            internals::hid::common::onSetFeature(report_id, buffer, len);
    }

    virtual void _onOutput(
        uint8_t report_id,
        const uint8_t *buffer,
        uint16_t len) override
    {
        // Note: never gets called unless report_id is zero. Reason unknown.
        internals::hid::common::onOutput(report_id, buffer, len);
    }
} simWheelHID;

static void usbEventCallback(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == ARDUINO_USB_EVENTS)
    {
        switch (event_id)
        {
        case ARDUINO_USB_STARTED_EVENT:
            log_i("USB PLUGGED");
            BLEAdvertising::stop();
            // This will trigger onBleConnectionStatus(false)
            BLEAdvertising::disconnect();
            break;
        case ARDUINO_USB_SUSPEND_EVENT:
            // DEVELOPER NOTE:
            // This should be "case ARDUINO_USB_STOPPED_EVENT:"
            // but I guess there is a bug in Arduino-ESP32
            log_i("USB UNPLUGGED");
            startBLEAdvertising();
            break;
        default:
            log_d("USB EVENT %u %u\n", event_base, event_id);
            break;
        }
    }
    else
        log_d("HID EVENT %u %u\n", event_base, event_id);
}

// ----------------------------------------------------------------------------
// BLE Server callbacks and advertising
// ----------------------------------------------------------------------------

void onBleConnectionStatus(bool connected)
{
    if (connected)
        OnConnected::notify();
    else
    {
        OnDisconnected::notify();
        if (!usbHidDevice.ready())
            startBLEAdvertising();
    }
}

uint16_t onInputReport(uint8_t reportId, uint8_t *data, uint16_t size)
{
    memcpy(data, inputReportData, size);
    return size;
}

void startBLEAdvertising()
{
    if (!BLEAdvertising::connected())
    {
        OnDisconnected::notify();
        BLEAdvertising::start();
    }
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
    if (!BLEDevice::initialized())
    {
        // BLE initialization
        BLEAdvertising::onConnectionStatus = onBleConnectionStatus;
        BLEHIDService::onReadInputReport = onInputReport;
        BLEHIDService::onGetFeatureReport =
            internals::hid::common::onGetFeature;
        BLEHIDService::onSetFeatureReport =
            internals::hid::common::onSetFeature;
        BLEHIDService::onWriteOutputReport =
            internals::hid::common::onOutput;
        BLEDeviceInfoService::manufacturer.name = deviceManufacturer;
        BLEDeviceInfoService::pnpInfo.vid = USB.VID();
        BLEDeviceInfoService::pnpInfo.pid = USB.PID();
        BLEDevice::init(deviceName);
    }

    if (!usbHidDevice.ready())
    {
        // USB Initialization
        USB.productName(deviceName.c_str());
        USB.manufacturerName(deviceManufacturer.c_str());
        USB.usbClass(0x03); // HID device class
        USB.usbSubClass(0); // No subclass
        uint64_t serialNumber;
        if (esp_efuse_mac_get_default((uint8_t *)(&serialNumber)) == ESP_OK)
        {
            char serialAsStr[9];
            snprintf(serialAsStr, 9, "%08llX", serialNumber);
            USB.serialNumber(serialAsStr);
        }
        usbHidDevice.addDevice(&simWheelHID, sizeof(hid_descriptor));
        USB.onEvent(usbEventCallback);
        usbHidDevice.onEvent(usbEventCallback);
        usbHidDevice.begin();
        USB.begin();
    }

    // Final touches
    startBLEAdvertising();
    internals::hid::reset();
}

// ----------------------------------------------------------------------------
// HID profile
// ----------------------------------------------------------------------------

void internals::hid::reset()
{
    internals::hid::common::onReset(inputReportData);
    if (BLEAdvertising::connected())
    {
        BLEHIDService::input_report.notify();
    }
    else if (usbHidDevice.ready())
    {
        usbHidDevice.SendReport(
            RID_INPUT_GAMEPAD,
            inputReportData,
            GAMEPAD_REPORT_SIZE);
    }
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
    {
        BLEHIDService::input_report.notify();
    }
    else if (usbHidDevice.ready())
    {
        usbHidDevice.SendReport(
            RID_INPUT_GAMEPAD,
            inputReportData,
            GAMEPAD_REPORT_SIZE);
    }
}

void internals::hid::reportBatteryLevel(const BatteryStatus &status)
{
    BatteryStatusChrData result =
        internals::hid::common::toBleBatteryStatus(status);
    BLEBatteryService::batteryStatus.set(result);
    BLEBatteryService::batteryLevel.set(result.battery_level);
}

void internals::hid::reportChangeInConfig()
{
    notifyConfigChanges = true; // Will be reported in the next input report
}

bool internals::hid::supportsCustomHardwareID() { return false; }

// ----------------------------------------------------------------------------
// Status
// ----------------------------------------------------------------------------

bool internals::hid::isConnected()
{
    return usbHidDevice.ready() || BLEAdvertising::connected();
}
