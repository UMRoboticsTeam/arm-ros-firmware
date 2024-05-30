// This example shows basic use of a Pololu High Power Stepper Motor Driver.
//
// It shows how to initialize the driver, configure various settings, and enable
// the driver.  It shows how to send pulses to the STEP pin to step the motor
// and how to switch directions using the DIR pin.
//
// Before using this example, be sure to change the setCurrentMilliamps36v4 line
// to have an appropriate current limit for your system.  Also, see this
// library's documentation for information about how to connect the driver:
//   http://pololu.github.io/high-power-stepper-driver

#include <SPI.h>
#include "ConnectionManager.hpp"

// This period is the length of the delay between steps, which controls the
// stepper motor's speed.  You can increase the delay to make the stepper motor
// go slower.  If you decrease the delay, the stepper motor will go faster, but
// there is a limit to how fast it can go before it starts missing steps.
const uint16_t StepPeriodUs = 2000;

static void ConnectionManager::init()
{
  SPI.begin();
}

uint8_t ConnectionManager::add_driver(uint8_t step_pin, uint8_t dir_pin, uint8_t cs_pin, uint16_t current, HPSDStepMode step_mode){
  // Create the HighPowerStepperDriver object, and save it and its info to the arrays
  HighPowerStepperDriver* sd = new HighPowerStepperDriver();
  this->drivers[this->num_drivers] = sd;
  this->step_pins[this->num_drivers] = step_pin;
  this->dir_pins[this->num_drivers] = dir_pin;
  
  sd->setChipSelectPin(cs_pin);

  // Drive the STEP and DIR pins low initially.
  pinMode(step_pin, OUTPUT);
  digitalWrite(step_pin, LOW);
  pinMode(dir_pin, OUTPUT);
  digitalWrite(dir_pin, LOW);

  // Give the driver some time to power up.
  delay(1);

  // TODO: Might want to wait until non-zero read from driver SPI connection

  // Reset the driver to its default settings and clear latched status
  // conditions.
  sd->resetSettings();
  sd->clearStatus();

  // Select auto mixed decay.  TI's DRV8711 documentation recommends this mode
  // for most applications, and we find that it usually works well.
  sd->setDecayMode(HPSDDecayMode::AutoMixed);

  // Set the current limit. You should change the number here to an appropriate
  // value for your particular system.
  sd->setCurrentMilliamps36v4(current);

  // Set the number of microsteps that correspond to one full step.
  sd->setStepMode(step_mode);

  // Enable the motor outputs.
  sd->enableDriver();

  return this->num_drivers++;
}

HighPowerStepperDriver* ConnectionManager::get_driver(uint8_t index){
  return index < this->num_drivers ? this->drivers[index] : NULL;
}

uint8_t ConnectionManager::get_step_pin(uint8_t index){
  return index < this->num_drivers ? this->step_pins[index] : NULL;
}

uint8_t ConnectionManager::get_dir_pin(uint8_t index){
  return index < this->num_drivers ? this->dir_pins[index] : NULL;
}

ConnectionManager::~ConnectionManager(){
  for (int i = 0; i < this->num_drivers; ++i)
  {
    delete this->drivers[i];
  }
  this->num_drivers = 0;
}
