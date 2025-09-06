#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tma340 {

using namespace touchscreen;

// Struct for the bootloader register from the Touchscreen Controller.
struct BootloaderData {
  uint8_t bl_file;
  uint8_t bl_status;
  uint8_t bl_error;
  uint8_t blver_hi;
  uint8_t blver_lo;
  uint8_t bld_blver_hi;
  uint8_t bld_blver_lo;
  uint8_t ttspver_hi;
  uint8_t ttspver_lo;
  uint8_t appid_hi;
  uint8_t appid_lo;
  uint8_t appver_hi;
  uint8_t appver_lo;
  uint8_t cid_0;
  uint8_t cid_1;
  uint8_t cid_2;
};

// Struct for the system info data registers from the Touchscreen Controller.
struct SysInfoData {
  uint8_t hst_mode;
  uint8_t mfg_stat;
  uint8_t mfg_cmd;
  uint8_t cid[3];
  uint8_t tt_undef1;
  uint8_t uid[8];
  uint8_t bl_verh;
  uint8_t bl_verl;
  uint8_t tts_verh;
  uint8_t tts_verl;
  uint8_t app_idh;
  uint8_t app_idl;
  uint8_t app_verh;
  uint8_t app_verl;
  uint8_t tt_undef[5];
  uint8_t scn_typ;
  uint8_t act_intrvl;
  uint8_t tch_tmout;
  uint8_t lp_intrvl;
};

class TMA340Touchscreen : public Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_rts_pin(GPIOPin *pin) { this->rts_pin_ = pin; }

  void set_power_state(bool enable);
  bool get_power_state();

 protected:
  void hard_reset_();
  bool soft_reset_();
  bool load_bootloader_regs_(struct BootloaderData *bootloader_data_ptr);
  bool exit_bootloader_mode_();
  bool set_sys_info_mode_(struct SysInfoData *sys_data_ptr);
  bool set_sys_info_regs_(struct SysInfoData *sys_data_ptr);
  void handshake_();
  void update_touches() override;

  InternalGPIOPin *interrupt_pin_;
  GPIOPin *rts_pin_;
  struct SysInfoData sys_data_;
};

}  // namespace tma340
}  // namespace esphome
