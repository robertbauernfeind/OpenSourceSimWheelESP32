/**
 * @file CustomSetup.ino
 *
 * @author Ángel Fernández Pineda. Madrid. Spain.
 * @date 2022-03-01
 * @brief Custom setup. Please, read
 *        [How to customize](../../../doc/hardware/CustomizeHowto_en.md)
 *
 * @copyright Licensed under the EUPL
 *
 */

#include "SimWheel.hpp"
#include "SimWheelUI.hpp"

//------------------------------------------------------------------
// Global customization
//------------------------------------------------------------------

/* -----------------------------------------------------------------
 >>>> [EN] DEVICE IDENTIFICATION
 >>>> [ES] IDENTIFICACIÓN DEL DISPOSITIVO
------------------------------------------------------------------ */

// [EN] Set a name for this device between double quotes
// [ES] Indique un nombre para este dispositivo entre comillas

std::string DEVICE_NAME = "BAR_GTE";

// [EN] Set a manufacturer's name for this device between double quotes
// [ES] Indique un nombre para el fabricante de este dispositivo entre comillas

std::string DEVICE_MANUFACTURER = "BAR";

//------------------------------------------------------------------
// Setup
//------------------------------------------------------------------

void simWheelSetup() {
  ShiftRegisterChip chip1, chip2, chip3, chip4;
  chip1[SR8Pin::A] = 1;
  chip1[SR8Pin::B] = 2;
  chip1[SR8Pin::C] = 3;
  chip1[SR8Pin::D] = 4;
  chip1[SR8Pin::E] = 5;
  chip1[SR8Pin::F] = 6;
  chip1[SR8Pin::G] = 7;
  chip1[SR8Pin::H] = 8;

  chip2[SR8Pin::A] = 10;
  chip2[SR8Pin::B] = 11;
  chip2[SR8Pin::C] = 12;
  chip2[SR8Pin::D] = 13;
  chip2[SR8Pin::E] = 14;
  chip2[SR8Pin::F] = 15;
  chip2[SR8Pin::G] = 16;
  chip2[SR8Pin::H] = 17;

  chip3[SR8Pin::A] = 20;
  chip3[SR8Pin::B] = 21;
  chip3[SR8Pin::C] = 22;
  chip3[SR8Pin::D] = 23;
  chip3[SR8Pin::E] = 24;
  chip3[SR8Pin::F] = 25;
  chip3[SR8Pin::G] = 26;
  chip3[SR8Pin::H] = 27;

  chip4[SR8Pin::A] = 30;
  chip4[SR8Pin::B] = 31;
  chip4[SR8Pin::C] = 32;
  chip4[SR8Pin::D] = 33;
  chip4[SR8Pin::E] = 34;
  chip4[SR8Pin::F] = 35;
  chip4[SR8Pin::G] = 36;
  chip4[SR8Pin::H] = 37;

  // inputs::add74HC165NChain(0, 1, 21, { chip1, chip2 }, 16, true);
  inputs::add74HC165NChain(3, 2, 4, { chip1, chip2, chip3, chip4 }, 32, false);
}

//------------------------------------------------------------------

void customFirmware() {
  simWheelSetup();
  hid::configure(
    DEVICE_NAME,
    DEVICE_MANUFACTURER);

  hid::connectivity(Connectivity::BLE);
}

//------------------------------------------------------------------
// Arduino entry point
//------------------------------------------------------------------

void setup() {
  firmware::run(customFirmware);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}