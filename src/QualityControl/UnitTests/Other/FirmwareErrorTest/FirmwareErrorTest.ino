/**
 * @file FirmwareErrorTest.ino
 *
 * @author Ángel Fernández Pineda. Madrid. Spain.
 * @date 2026-01-12
 * @brief Unit Test. See [README](./README.md)
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "SimWheel.hpp"
#include "SimWheelInternals.hpp"
#include <stdexcept>

//------------------------------------------------------------------
// Mocks
//------------------------------------------------------------------

void internals::storage::getReady() {}
void internals::hid::common::getReady() {}
void internals::inputMap::getReady() {}
void internals::inputHub::getReady() {}
void internals::inputs::getReady() {}
void internals::batteryCalibration::getReady() {}
void internals::batteryMonitor::getReady() {}
void internals::power::getReady() {}
void internals::pixels::getReady() {}
void internals::ui::getReady() {}

void customFirmware()
{
    throw ::std::runtime_error("TEST MESSAGE");
}

//------------------------------------------------------------------
// Arduino entry point
//------------------------------------------------------------------

void setup()
{
    firmware::run(customFirmware);
}

//------------------------------------------------------------------

void loop() {}
