#include "tma340.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <vector>

namespace esphome {
namespace tma340 {

static const char *const TAG = "tma340";

// TMA340 Touch IC I2C address (7 bit I2C address).
static const uint8_t I2C_Addr = 0x24;
// TMA340 touchscreen controller I2C regs.
static const uint8_t BASE_ADDR = 0x00;
static const uint8_t SOFT_RST_MODE = 0x01;
static const uint8_t SYSINFO_MODE = 0x10;
static const uint8_t OPERATE_MODE = 0x00;
static const uint8_t LOW_POWER_MODE = 0x04;
static const uint8_t DEEP_SLEEP_MODE = 0x02;
static const uint8_t REG_ACT_INTRVL = 0x1D;
// Active Power state scanning/processing refresh interval
static const uint8_t ACT_INTRVL_DFLT = 0x00;
// Low Power state scanning/processing refresh interval
static const uint8_t LP_INTRVL_DFLT = 0x0A;
// Touch timeout for the Active power */
static const uint8_t TCH_TMOUT_DFLT = 0xFF;
// Max X and Y sizes reported by the TSC.
static const uint16_t MAX_X = 682;
static const uint16_t MAX_Y = 1023;

static const uint8_t BOOTLOADER_COMMAND[11] = {0x00, 0xFF, 0xA5, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

void TMA340Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  this->rts_pin_->setup();

  this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
  this->interrupt_pin_->setup();

  this->rts_pin_->digital_write(true);
  delay(50);
  hard_reset_();

  if (!soft_reset_()) {
    ESP_LOGE(TAG, "Failed to soft reset TMA340!");
    this->mark_failed();
    return;
  }

  if (!exit_bootloader_mode_()) {
    ESP_LOGE(TAG, "Failed to exit bootloader mode!");
    this->mark_failed();
    return;
  }

  if (!set_sys_info_mode_(&sys_data_)) {
    ESP_LOGE(TAG, "Failed to set sys info mode!");
    this->mark_failed();
    return;
  }

  if (!set_sys_info_regs_(&sys_data_)) {
    ESP_LOGE(TAG, "Failed to set sys info regs!");
    this->mark_failed();
    return;
  }

  this->write_register(BASE_ADDR, &OPERATE_MODE, 1);

  uint8_t dist_default_value = 0xF8;
  this->write_register(0x1E, &dist_default_value, 1);

  this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);

  this->x_raw_max_ = MAX_X;
  this->y_raw_max_ = MAX_Y;

  if (this->swap_x_y_)
    std::swap(this->x_raw_max_, this->y_raw_max_);

  delay(50);
}

void TMA340Touchscreen::update_touches() {
  uint8_t touch_count = 0;
  int16_t x_raw, y_raw;
  uint8_t _regs[32];

  if (this->read_register(BASE_ADDR, _regs, sizeof(_regs)) != i2c::ERROR_OK)
    return;

  handshake_();

  touch_count = std::min<uint8_t>(_regs[2], 2);

  ESP_LOGV(TAG, "Touch count: %d", touch_count);
  uint16_t base = 3;
  for (uint8_t i = 0; i < touch_count; i++) {
    // flip y and swap x and y
    x_raw = _regs[base] << 8 | _regs[base + 1];
    y_raw = _regs[base + 2] << 8 | _regs[base + 3];
    this->add_raw_touch_position_(i, x_raw, y_raw);
    base *= 3;
  }
}

void TMA340Touchscreen::set_power_state(bool enable) {
  uint8_t state = OPERATE_MODE;
  if (!enable)
    state = DEEP_SLEEP_MODE;
  this->write_register(BASE_ADDR, &state, 1);
}

bool TMA340Touchscreen::get_power_state() {
  uint8_t state;
  this->read_register(BASE_ADDR, &state, 1);
  bool ret = 0;
  switch (state) {
    case DEEP_SLEEP_MODE:
      ret = 0;
      break;
    case LOW_POWER_MODE:
    case OPERATE_MODE:
      ret = 1;
      break;
  }
  return ret;
}

void TMA340Touchscreen::hard_reset_() {
  this->rts_pin_->digital_write(true);
  delay(10);
  this->rts_pin_->digital_write(false);
  delay(2);
  this->rts_pin_->digital_write(true);
  delay(10);
}

bool TMA340Touchscreen::soft_reset_() {
  if (this->write_register(BASE_ADDR, &SOFT_RST_MODE, 1) != i2c::ERROR_OK)
    return false;

  delay(20);
  return true;
}

bool TMA340Touchscreen::load_bootloader_regs_(struct BootloaderData *bootloader_data_ptr) {
  uint8_t bootloader_data[16];

  if (this->read_register(BASE_ADDR, bootloader_data, 16) != i2c::ERROR_OK)
    return false;
  memcpy(bootloader_data_ptr, bootloader_data, 16);

  return true;
}

bool TMA340Touchscreen::exit_bootloader_mode_() {
  this->write_register(BASE_ADDR, BOOTLOADER_COMMAND, sizeof(BOOTLOADER_COMMAND));

  delay(500);
  struct BootloaderData bootloader_data;
  load_bootloader_regs_(&bootloader_data);
  if (((bootloader_data.bl_status) & 0x10) >> 4)
    return false;

  return true;
}

bool TMA340Touchscreen::set_sys_info_mode_(struct SysInfoData *sys_data_ptr) {
  this->write_register(BASE_ADDR, &SYSINFO_MODE, 1);
  delay(20);

  uint8_t sys_info_array[32];
  if (this->read_register(BASE_ADDR, sys_info_array, sizeof(sys_info_array)) != i2c::ERROR_OK)
    return false;

  memcpy(sys_data_ptr, sys_info_array, sizeof(sys_info_array));

  handshake_();
  if (!sys_data_ptr->tts_verh && !sys_data_ptr->tts_verl)
    return false;

  return true;
}

bool TMA340Touchscreen::set_sys_info_regs_(struct SysInfoData *sys_data_ptr) {
  sys_data_ptr->act_intrvl = ACT_INTRVL_DFLT;
  sys_data_ptr->tch_tmout = TCH_TMOUT_DFLT;
  sys_data_ptr->lp_intrvl = LP_INTRVL_DFLT;

  uint8_t _regs[] = {sys_data_ptr->act_intrvl, sys_data_ptr->tch_tmout, sys_data_ptr->lp_intrvl};

  if (this->write_register(0x1D, _regs, 3) != i2c::ERROR_OK)
    return false;

  delay(20);
  return true;
}

void TMA340Touchscreen::handshake_() {
  uint8_t handshake_mode_reg = 0;
  this->read_register(BASE_ADDR, &handshake_mode_reg, 1);
  handshake_mode_reg ^= 0x80;
  this->write_register(BASE_ADDR, &handshake_mode_reg, 1);
}

void TMA340Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "TMA340 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  RTS Pin: ", this->rts_pin_);
}

}  // namespace tma340
}  // namespace esphome
