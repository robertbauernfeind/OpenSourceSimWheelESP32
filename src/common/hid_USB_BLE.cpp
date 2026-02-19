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
#include "esp_mac.h" // For esp_efuse_mac_get_default()
#include <cstring>
// #include <Arduino.h> // For debugging

// ----------------------------------------------------------------------------
// USB classes
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
        if (BLEAdvertising::connected())
            // BLE takes precedence
            return;
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
        if (BLEAdvertising::connected())
            // BLE takes precedence
            return;
        // Note: never gets called unless report_id is zero. Reason unknown.
        internals::hid::common::onOutput(report_id, buffer, len);
    }
};

// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

static uint8_t inputReportData[GAMEPAD_REPORT_SIZE] = {0};
static bool notifyConfigChanges = false;
USBHID usbHidDevice;
SimWheelHIDImpl simWheelHID;

// ----------------------------------------------------------------------------
// BLE Server callbacks and advertising
// ----------------------------------------------------------------------------

void onConnectionStatus(bool connected)
{
    if (connected)
        usbHidDevice.end();
    else
    {
        usbHidDevice.begin();
        BLEAdvertising::start();
    }
}

void startBLEAdvertising()
{
    onConnectionStatus(false);
}

uint16_t onInputReport(uint8_t reportId, uint8_t *data, uint16_t size)
{
    memcpy(data, inputReportData, size);
    return size;
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
        BLEAdvertising::onConnectionStatus = onConnectionStatus;
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
        USB.begin();
    }

    // Final touches
    startBLEAdvertising();
    OnConnected::notify();
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
