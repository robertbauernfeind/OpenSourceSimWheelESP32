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

// Related to the connection state machine
#define STATE_NOT_CONNECTED 0
#define STATE_BLE_CONNECTED 1
#define STATE_USB_CONNECTED 2
#define STATE_INITIAL 255
static uint8_t connection_state = STATE_INITIAL;

// Related to automatic shutdown
static esp_timer_handle_t autoPowerOffTimer = nullptr;

// Related to HID
static uint8_t inputReportData[GAMEPAD_REPORT_SIZE] = {0};
static bool notifyConfigChanges = false;
USBHID usbHid;

// We create a new USB instance as the default stack size is not enough
ESPUSB USB_instance(4096);
#define USB USB_instance

// ----------------------------------------------------------------------------
// Automatic shutdown
// ----------------------------------------------------------------------------

void autoPowerOffCallback(void *unused)
{
    PowerService::call::shutdown();
}

// ----------------------------------------------------------------------------
// State machine for connection status
//
// stateDiagram-v2
//   state "Not connected" as not_connected
//   state "BLE connected, USB disconnected" as ble
//   state "USB connected, BLE disconnected" as usb
//   [*] --> not_connected
//   not_connected --> [*]: timeout
//   not_connected --> ble: BLE connection
//   not_connected --> usb: USB connection
//   usb --> not_connected: USB disconnection
//   ble --> not_connected: BLE disconnection
// ----------------------------------------------------------------------------

void new_connection_state(uint8_t new_state)
{
    switch (new_state)
    {
    case STATE_NOT_CONNECTED:
    {
        connection_state = STATE_NOT_CONNECTED;
        OnDisconnected::notify();
        BLEAdvertising::start();
        if (autoPowerOffTimer != nullptr)
            esp_timer_start_once(
                autoPowerOffTimer,
                AUTO_POWER_OFF_DELAY_SECS * 1000000);
    }
    break;
    case STATE_BLE_CONNECTED:
    {
        if (autoPowerOffTimer != nullptr)
            esp_timer_stop(autoPowerOffTimer);
        connection_state = STATE_BLE_CONNECTED;
        OnConnected::notify();
    }
    break;
    case STATE_USB_CONNECTED:
    {
        if (autoPowerOffTimer != nullptr)
            esp_timer_stop(autoPowerOffTimer);
        // Disable BLE connectivity
        connection_state = STATE_USB_CONNECTED;
        BLEAdvertising::stop();
        // Note: this may trigger onBleConnectionStatus(false)
        // so we have to change the state before it gets called
        BLEAdvertising::disconnect();
        OnConnected::notify();
    }
    break;
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
// USB
// ----------------------------------------------------------------------------

class SimWheelUsbHid : public USBHIDDevice
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

public:
    static void event_callback(
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
                new_connection_state(STATE_USB_CONNECTED);
                break;
            case ARDUINO_USB_SUSPEND_EVENT:
                // DEVELOPER NOTE:
                // This should be "case ARDUINO_USB_STOPPED_EVENT:"
                // but I guess there is a bug in Arduino-ESP32
                log_i("USB UNPLUGGED");
                new_connection_state(STATE_NOT_CONNECTED);
                break;
            default:
                log_d("USB EVENT %u %u\n", event_base, event_id);
                break;
            }
        }
        else
            log_d("HID EVENT %u %u\n", event_base, event_id);
    }

} usbSimWheel;

// ----------------------------------------------------------------------------
// BLE
// ----------------------------------------------------------------------------

void onBleConnectionStatus(bool connected)
{
    if (connected)
        new_connection_state(STATE_BLE_CONNECTED);
    else if (connection_state == STATE_BLE_CONNECTED)
        new_connection_state(STATE_NOT_CONNECTED);
    // else the disconnection was forced by ARDUINO_USB_STARTED_EVENT
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
    internals::hid::common::onReset(inputReportData);

    if (enableAutoPowerOff && !autoPowerOffTimer)
    {
        esp_timer_create_args_t args;
        args.callback = &autoPowerOffCallback;
        args.arg = nullptr;
        args.name = nullptr;
        args.dispatch_method = ESP_TIMER_TASK;
        ESP_ERROR_CHECK(esp_timer_create(&args, &autoPowerOffTimer));
    }

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

    if (!usbHid.ready())
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
        usbHid.addDevice(&usbSimWheel, sizeof(hid_descriptor));
        // Note:
        // event_callback() must be subscribed to both USB and usbHid.
        // Otherwise USB events are not received.
        // Probably a bug in Arduino-ESP32.
        USB.onEvent(SimWheelUsbHid::event_callback);
        usbHid.onEvent(SimWheelUsbHid::event_callback);
        usbHid.begin();
        USB.begin();
    }

    new_connection_state(STATE_NOT_CONNECTED);
}

// ----------------------------------------------------------------------------
// HID profile
// ----------------------------------------------------------------------------

void internals::hid::reset()
{
    internals::hid::common::onReset(inputReportData);
    if (connection_state == STATE_BLE_CONNECTED)
    {
        BLEHIDService::input_report.notify();
    }
    else if (connection_state == STATE_USB_CONNECTED)
    {
        usbHid.SendReport(
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
    if (connection_state == STATE_BLE_CONNECTED)
    {
        BLEHIDService::input_report.notify();
    }
    else if (connection_state == STATE_USB_CONNECTED)
    {
        usbHid.SendReport(
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
    return usbHid.ready() || BLEAdvertising::connected();
}
