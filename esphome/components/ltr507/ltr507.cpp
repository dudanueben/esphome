#include "ltr507.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

using esphome::i2c::ErrorCode;

namespace esphome {
namespace ltr507 {

static const char *const TAG = "ltr507";

static const uint8_t MAX_TRIES = 5;
static const uint8_t MAX_SENSITIVITY_ADJUSTMENTS = 10;

struct GainTimePair {
  AlsGain507 gain;
  IntegrationTime507 time;
};

bool operator==(const GainTimePair &lhs, const GainTimePair &rhs) {
  return lhs.gain == rhs.gain && lhs.time == rhs.time;
}

bool operator!=(const GainTimePair &lhs, const GainTimePair &rhs) {
  return lhs.gain != rhs.gain || lhs.time != rhs.time;
}

template<typename T, size_t size> T get_next(const T (&array)[size], const T val) {
  size_t i = 0;
  size_t idx = -1;
  while (idx == -1 && i < size) {
    if (array[i] == val) {
      idx = i;
      break;
    }
    i++;
  }
  if (idx == -1 || i + 1 >= size)
    return val;
  return array[i + 1];
}

template<typename T, size_t size> T get_prev(const T (&array)[size], const T val) {
  size_t i = size - 1;
  size_t idx = -1;
  while (idx == -1 && i > 0) {
    if (array[i] == val) {
      idx = i;
      break;
    }
    i--;
  }
  if (idx == -1 || i == 0)
    return val;
  return array[i - 1];
}

static float get_itime_ms(IntegrationTime507 time) {
  static const float ALS_INT_TIME[TIMES_COUNT] = {12000.0f, 600.0f, 300.0f, 150.0f, 75.0f, 4.685f, 0.292f, 0.018f};
  return ALS_INT_TIME[time & 0b111];
}

static uint16_t get_meas_time_ms(MeasurementRepeatRate507 rate) {
  static const uint16_t ALS_MEAS_RATE[8] = {100, 200, 500, 1000, 2000, 2000, 2000, 2000};
  return ALS_MEAS_RATE[rate & 0b111];
}

static float get_gain_coeff(AlsGain507 gain) {
  static const uint16_t ALS_GAIN[GAINS_COUNT] = {1, 2, 100, 200};
  return ALS_GAIN[gain & 0b11];
}

void LTRAlsPs507Component::setup() {
  // As per datasheet we need to wait at least 100ms after power on to get ALS chip responsive
  this->set_timeout(100, [this]() { this->state_ = State::DELAYED_SETUP; });
}

void LTRAlsPs507Component::dump_config() {
  auto get_device_type = [](LtrType typ) {
    switch (typ) {
      case LtrType::LTR_TYPE_ALS_ONLY:
        return "ALS only";
      case LtrType::LTR_TYPE_PS_ONLY:
        return "PS only";
      case LtrType::LTR_TYPE_ALS_AND_PS:
        return "Als + PS";
      default:
        return "Unknown";
    }
  };

  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG,
                "  Device type: %s\n"
                "  Automatic mode: %s\n"
                "  Gain: %.0fx\n"
                "  Integration time: %.3f ms\n"
                "  Measurement repeat rate: %d ms\n"
                "  Glass attenuation factor: %f\n"
                "  Proximity cooldown time: %d s\n"
                "  Proximity high threshold: %d\n"
                "  Proximity low threshold: %d",
                get_device_type(this->ltr_type_), ONOFF(this->automatic_mode_enabled_), get_gain_coeff(this->gain_),
                get_itime_ms(this->integration_time_), get_meas_time_ms(this->repeat_rate_),
                this->glass_attenuation_factor_, this->ps_cooldown_time_s_, this->ps_threshold_high_,
                this->ps_threshold_low_);

  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "ALS calculated lux", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "CH2 Infrared counts", this->infrared_counts_sensor_);
  LOG_SENSOR("  ", "CH1 Visible+IR counts", this->full_spectrum_counts_sensor_);
  LOG_SENSOR("  ", "Actual gain", this->actual_gain_sensor_);

  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

void LTRAlsPs507Component::update() {
  if (!this->is_als_()) {
    ESP_LOGW(TAG, "Update. ALS data not available. Change configuration to ALS or ALS_PS.");
    return;
  }
  if (this->is_ready() && this->is_als_() && this->state_ == State::IDLE) {
    ESP_LOGV(TAG, "Update. Initiating new ALS data collection.");

    this->state_ = this->automatic_mode_enabled_ ? State::COLLECTING_DATA_AUTO : State::WAITING_FOR_DATA;

    this->als_readings_.ch1 = 0;
    this->als_readings_.ch2 = 0;
    this->als_readings_.ch1_coeff = 0;
    this->als_readings_.ch2_coeff = 0;
    this->als_readings_.gain = this->gain_;
    this->als_readings_.integration_time = this->integration_time_;
    this->als_readings_.lux = 0;
    this->als_readings_.number_of_adjustments = 0;

  } else {
    ESP_LOGV(TAG, "Update. Component not ready yet.");
  }
}

void LTRAlsPs507Component::loop() {
  ErrorCode err = i2c::ERROR_OK;
  static uint8_t tries{0};

  switch (this->state_) {
    case State::DELAYED_SETUP:
      err = this->write(nullptr, 0);
      if (err != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "i2c connection failed");
        this->mark_failed();
      }
      this->configure_reset_();
      if (this->is_als_()) {
        this->configure_als_();
        this->configure_integration_time_(this->integration_time_);
      }
      if (this->is_ps_()) {
        this->configure_ps_();
      }

      this->state_ = State::IDLE;
      break;

    case State::IDLE:
      if (this->is_ps_()) {
        this->check_and_trigger_ps_();
      }
      break;

    case State::WAITING_FOR_DATA:
      if (this->is_als_data_ready_(this->als_readings_) == DataAvail::DATA_OK) {
        tries = 0;
        ESP_LOGV(TAG, "Reading sensor data assuming gain = %.0fx, time = %.3f ms",
                 get_gain_coeff(this->als_readings_.gain), get_itime_ms(this->als_readings_.integration_time));
        this->read_sensor_data_(this->als_readings_);
        this->apply_gain_coeff_(this->als_readings_);
        this->state_ = State::DATA_COLLECTED;
      } else if (tries >= MAX_TRIES) {
        ESP_LOGW(TAG, "Can't get data after several tries. Aborting.");
        tries = 0;
        this->status_set_warning();
        this->state_ = State::IDLE;
        return;
      } else {
        tries++;
      }
      break;

    case State::COLLECTING_DATA_AUTO:
    case State::DATA_COLLECTED:
      // first measurement in auto mode (COLLECTING_DATA_AUTO state) require device reconfiguration
      if (this->state_ == State::COLLECTING_DATA_AUTO || this->are_adjustments_required_(this->als_readings_)) {
        this->state_ = State::ADJUSTMENT_IN_PROGRESS;
        ESP_LOGD(TAG, "Reconfiguring sensitivity: gain = %.0fx, time = %.3f ms",
                 get_gain_coeff(this->als_readings_.gain), get_itime_ms(this->als_readings_.integration_time));
        this->configure_integration_time_(this->als_readings_.integration_time);
        this->configure_gain_(this->als_readings_.gain);
        // if sensitivity adjustment needed - need to wait for first data samples after setting new parameters
        this->set_timeout(2 * get_meas_time_ms(this->repeat_rate_),
                          [this]() { this->state_ = State::WAITING_FOR_DATA; });
      } else {
        this->state_ = State::READY_TO_PUBLISH;
      }
      break;

    case State::ADJUSTMENT_IN_PROGRESS:
      // nothing to be done, just waiting for the timeout
      break;

    case State::READY_TO_PUBLISH:
      this->publish_data_part_1_(this->als_readings_);
      this->state_ = State::KEEP_PUBLISHING;
      break;

    case State::KEEP_PUBLISHING:
      this->publish_data_part_2_(this->als_readings_);
      this->status_clear_warning();
      this->state_ = State::IDLE;
      break;

    default:
      break;
  }
}

void LTRAlsPs507Component::check_and_trigger_ps_() {
  static uint32_t last_high_trigger_time{0};
  static uint32_t last_low_trigger_time{0};
  uint16_t ps_data = this->read_ps_data_();
  uint32_t now = millis();

  if (ps_data != this->ps_readings_) {
    this->ps_readings_ = ps_data;
    // Higher values - object is closer to sensor
    if (ps_data > this->ps_threshold_high_ && now - last_high_trigger_time >= this->ps_cooldown_time_s_ * 1000) {
      last_high_trigger_time = now;
      ESP_LOGD(TAG, "Proximity high threshold triggered. Value = %d, Trigger level = %d", ps_data,
               this->ps_threshold_high_);
      this->on_ps_high_trigger_callback_.call();
    } else if (ps_data < this->ps_threshold_low_ && now - last_low_trigger_time >= this->ps_cooldown_time_s_ * 1000) {
      last_low_trigger_time = now;
      ESP_LOGD(TAG, "Proximity low threshold triggered. Value = %d, Trigger level = %d", ps_data,
               this->ps_threshold_low_);
      this->on_ps_low_trigger_callback_.call();
    }
  }
}

bool LTRAlsPs507Component::check_part_number_() {
  uint8_t manuf_id = this->reg((uint8_t) CommandRegisters::MANUFAC_ID).get();
  if (manuf_id != 0x05) {  // 0x05 is Lite-On Semiconductor Corp. ID
    ESP_LOGW(TAG, "Unknown manufacturer ID: 0x%02X", manuf_id);
    this->mark_failed();
    return false;
  }

  // Things getting not really funny here, we can't identify device type by part number ID
  // ======================== ========= ===== =================
  // Device                    Part ID   Rev   Capabilities
  // ======================== ========= ===== =================
  // ltr-507als                 0x09      0    als + ps

  PartIdRegister part_id{0};
  part_id.raw = this->reg((uint8_t) CommandRegisters::PART_ID).get();
  if (part_id.part_number_id != 0x09) {
    ESP_LOGW(TAG, "Unknown part number ID: 0x%02X. LTR-507 shall have 0x09. It might not work properly.",
             part_id.part_number_id);
    this->status_set_warning();
    return true;
  }
  return true;
}

void LTRAlsPs507Component::configure_reset_() {
  ESP_LOGV(TAG, "Resetting");

  AlsControlRegister507 als_ctrl{0};
  als_ctrl.sw_reset = true;
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(2);

  uint8_t tries = MAX_TRIES;
  do {
    ESP_LOGV(TAG, "Waiting chip to reset");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  } while (als_ctrl.sw_reset && tries--);  // while sw reset bit is on - keep waiting

  if (als_ctrl.sw_reset) {
    ESP_LOGW(TAG, "Reset failed");
  }
}

void LTRAlsPs507Component::configure_als_() {
  AlsControlRegister507 als_ctrl{0};
  als_ctrl.sw_reset = false;
  als_ctrl.als_mode_active = true;
  als_ctrl.gain = this->gain_;

  ESP_LOGV(TAG, "Setting active mode and gain reg 0x%02X", als_ctrl.raw);
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(5);

  uint8_t tries = MAX_TRIES;
  do {
    ESP_LOGV(TAG, "Waiting for ALS device to become active");
    delay(2);
    als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  } while (!als_ctrl.als_mode_active && tries--);  // while active mode is not set - keep waiting

  if (!als_ctrl.als_mode_active) {
    ESP_LOGW(TAG, "Failed to activate ALS device");
  }
}

void LTRAlsPs507Component::configure_ps_() {
  PsMeasurementRateRegister ps_meas{0};
  ps_meas.ps_measurement_rate = PsMeasurementRate507::PS_MEAS_RATE_100MS;
  this->reg((uint8_t) CommandRegisters::PS_MEAS_RATE) = ps_meas.raw;

  PsControlRegister507 ps_ctrl{0};
  ps_ctrl.ps_mode_active = true;
  this->reg((uint8_t) CommandRegisters::PS_CONTR) = ps_ctrl.raw;
}

uint16_t LTRAlsPs507Component::read_ps_data_() {
  AlsPsStatusRegister als_status{0};
  als_status.raw = this->reg((uint8_t) CommandRegisters::ALS_PS_STATUS).get();
  if (!als_status.ps_new_data) {
    return this->ps_readings_;
  }

  uint8_t ps_low = this->reg((uint8_t) CommandRegisters::PS_DATA_0).get();
  PsData1Register ps_high;
  ps_high.raw = this->reg((uint8_t) CommandRegisters::PS_DATA_1).get();

  uint16_t val = encode_uint16(ps_high.ps_data_high, ps_low);
  return val;
}

void LTRAlsPs507Component::configure_gain_(AlsGain507 gain) {
  AlsControlRegister507 als_ctrl{0};
  als_ctrl.als_mode_active = true;
  als_ctrl.gain = gain;
  this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
  delay(2);

  AlsControlRegister507 read_als_ctrl{0};
  read_als_ctrl.raw = this->reg((uint8_t) CommandRegisters::ALS_CONTR).get();
  if (read_als_ctrl.gain != gain) {
    ESP_LOGW(TAG, "Failed to set gain. We will try one more time.");
    this->reg((uint8_t) CommandRegisters::ALS_CONTR) = als_ctrl.raw;
    delay(2);
  }
}

void LTRAlsPs507Component::configure_integration_time_(IntegrationTime507 time) {
  MeasurementRateRegister507 meas{0};
  meas.measurement_repeat_rate = this->repeat_rate_;
  meas.integration_time = time;
  this->reg((uint8_t) CommandRegisters::MEAS_RATE) = meas.raw;
  delay(2);

  MeasurementRateRegister507 read_meas{0};
  read_meas.raw = this->reg((uint8_t) CommandRegisters::MEAS_RATE).get();
  if (read_meas.integration_time != time) {
    ESP_LOGW(TAG, "Failed to set integration time. We will try one more time.");
    this->reg((uint8_t) CommandRegisters::MEAS_RATE) = meas.raw;
    delay(2);
  }
}

DataAvail LTRAlsPs507Component::is_als_data_ready_(AlsReadings &data) {
  AlsPsStatusRegister als_status{0};
  als_status.raw = this->reg((uint8_t) CommandRegisters::ALS_PS_STATUS).get();
  if (!als_status.als_new_data)
    return DataAvail::NO_DATA;
  return DataAvail::DATA_OK;
}

void LTRAlsPs507Component::read_sensor_data_(AlsReadings &data) {
  data.ch1 = 0;
  data.ch2 = 0;
  uint8_t ch1_0 = ((AlsDataChX0Register) this->reg((uint8_t) CommandRegisters::ALS_DATA_CH1_0).get()).als_data_chx_0;
  uint8_t ch1_1 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH1_1).get();
  uint8_t ch1_2 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH1_2).get();
  uint8_t ch2_0 = ((AlsDataChX0Register) this->reg((uint8_t) CommandRegisters::ALS_DATA_CH2_0).get()).als_data_chx_0;
  uint8_t ch2_1 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH2_1).get();
  uint8_t ch2_2 = this->reg((uint8_t) CommandRegisters::ALS_DATA_CH2_2).get();
  data.ch1 = (static_cast<uint32_t>(encode_uint16(ch1_2, ch1_1)) << 4) | (static_cast<uint32_t>(ch1_0));
  data.ch2 = (static_cast<uint32_t>(encode_uint16(ch2_2, ch2_1)) << 4) | (static_cast<uint32_t>(ch2_0));

  // read coefficients
  data.ch1_coeff = 0;
  data.ch2_coeff = 0;
  uint8_t ch1_coeff_0 = this->reg((uint8_t) CommandRegisters::ALS_COEFF1_DATA_0).get();
  uint8_t ch1_coeff_1 = this->reg((uint8_t) CommandRegisters::ALS_COEFF1_DATA_1).get();
  uint8_t ch2_coeff_0 = this->reg((uint8_t) CommandRegisters::ALS_COEFF2_DATA_0).get();
  uint8_t ch2_coeff_1 = this->reg((uint8_t) CommandRegisters::ALS_COEFF2_DATA_1).get();
  data.ch1_coeff = encode_uint16(ch1_coeff_1, ch1_coeff_0);
  data.ch2_coeff = encode_uint16(ch2_coeff_1, ch2_coeff_0);

  // read integer lux
  data.lux = 0.0f;
  uint8_t lux_0 = this->reg((uint8_t) CommandRegisters::ALS_DATA_0).get();
  uint8_t lux_1 = this->reg((uint8_t) CommandRegisters::ALS_DATA_1).get();
  data.lux = static_cast<float>(encode_uint16(lux_1, lux_0));

  ESP_LOGD(TAG, "Got sensor data: CH1 = %d, CH2 = %d, CH1 Coeff = %d, CH2 Coeff = %d, Lux = %f", data.ch1, data.ch2,
           data.ch1_coeff, data.ch2_coeff, data.lux);
}

bool LTRAlsPs507Component::are_adjustments_required_(AlsReadings &data) {
  if (!this->automatic_mode_enabled_)
    return false;

  if (data.number_of_adjustments > 15) {
    // sometimes sensors fail to change sensitivity. this prevents us from infinite loop
    ESP_LOGW(TAG, "Too many sensitivity adjustments done. Apparently, sensor reconfiguration fails. Stopping.");
    return false;
  }
  data.number_of_adjustments++;

  // Recommended thresholds as per datasheet
  static const uint16_t LOW_INTENSITY_THRESHOLD = 1000;
  static const uint16_t HIGH_INTENSITY_THRESHOLD = 30000;
  static const AlsGain507 GAINS[GAINS_COUNT] = {GAIN_1, GAIN_2, GAIN_100, GAIN_200};
  static const IntegrationTime507 INT_TIMES[TIMES_COUNT] = {
      INTEGRATION_TIME_1200MS, INTEGRATION_TIME_600MS, INTEGRATION_TIME_300MS, INTEGRATION_TIME_150MS,
      INTEGRATION_TIME_75MS,   INTEGRATION_TIME_5MS,   INTEGRATION_TIME_292US, INTEGRATION_TIME_18US};

  if (data.ch1 <= LOW_INTENSITY_THRESHOLD) {
    AlsGain507 next_gain = get_next(GAINS, data.gain);
    if (next_gain != data.gain) {
      data.gain = next_gain;
      ESP_LOGV(TAG, "Low illuminance. Increasing gain.");
      return true;
    }
    IntegrationTime507 next_time = get_prev(INT_TIMES, data.integration_time);
    if (next_time != data.integration_time) {
      data.integration_time = next_time;
      ESP_LOGV(TAG, "Low illuminance. Increasing integration time.");
      return true;
    }
  } else if (data.ch1 >= HIGH_INTENSITY_THRESHOLD) {
    AlsGain507 prev_gain = get_prev(GAINS, data.gain);
    if (prev_gain != data.gain) {
      data.gain = prev_gain;
      ESP_LOGV(TAG, "High illuminance. Decreasing gain.");
      return true;
    }
    IntegrationTime507 prev_time = get_next(INT_TIMES, data.integration_time);
    if (prev_time != data.integration_time) {
      data.integration_time = prev_time;
      ESP_LOGV(TAG, "High illuminance. Decreasing integration time.");
      return true;
    }
  } else {
    ESP_LOGD(TAG, "Illuminance is sufficient.");
    return false;
  }
  ESP_LOGD(TAG, "Can't adjust sensitivity anymore.");
  return false;
}

void LTRAlsPs507Component::apply_gain_coeff_(AlsReadings &data) {
  float als_gain = get_gain_coeff(data.gain);
  data.lux = data.lux / als_gain;

  ESP_LOGD(TAG, "Lux calculation: gain %.0f, lux %.3f", als_gain, data.lux);
}

void LTRAlsPs507Component::apply_lux_calculation_(AlsReadings &data) {
  if ((data.ch1 == 0xFFFFF) || (data.ch2 == 0xFFFFF)) {
    ESP_LOGW(TAG, "Sensors got saturated");
    data.lux = 0.0f;
    return;
  }

  if ((data.ch1 == 0x00000) && (data.ch2 == 0x00000)) {
    ESP_LOGW(TAG, "Sensors blacked out");
    data.lux = 0.0f;
    return;
  }

  float ch1 = data.ch1;
  float ch2 = data.ch2;
  float ch1_coeff = data.ch1_coeff;
  float ch2_coeff = data.ch2_coeff;
  float ratio = ch2 / (ch1 + ch2);
  float als_gain = get_gain_coeff(data.gain);
  float als_time = (get_itime_ms(data.integration_time)) / 100.0f;
  float inv_pfactor = this->glass_attenuation_factor_;
  float lux = 0.0f;

  // method from
  // https://github.com/fards/Ainol_fire_kernel/blob/83832cf8a3082fd8e963230f4b1984479d1f1a84/customer/drivers/lightsensor/ltr501als.c#L295

  if (ratio < 0.45) {
    lux = 1.7743 * ch1 + 1.1059 * ch2;
  } else if (ratio < 0.64) {
    lux = 3.7725 * ch1 - 1.3363 * ch2;
  } else if (ratio < 0.85) {
    lux = 1.6903 * ch2 - 0.1693 * ch2;
  } else {
    ESP_LOGW(TAG, "Impossible ch2/(ch1 + ch2) ratio");
    lux = 0.0f;
  }

  lux = inv_pfactor * lux / als_gain / als_time;

  ESP_LOGD(TAG,
           "Lux calculation: ratio %.3f, gain %.0fx, int time %.1f, inv_pfactor %.3f, lux %.3f, lux from sensor: %.3f",
           ratio, als_gain, als_time, inv_pfactor, lux, data.lux);

  data.lux = lux;
}

void LTRAlsPs507Component::publish_data_part_1_(AlsReadings &data) {
  if (this->proximity_counts_sensor_ != nullptr) {
    this->proximity_counts_sensor_->publish_state(this->ps_readings_);
  }
  if (this->ambient_light_sensor_ != nullptr) {
    this->ambient_light_sensor_->publish_state(data.lux);
  }
  if (this->infrared_counts_sensor_ != nullptr) {
    this->infrared_counts_sensor_->publish_state(data.ch2);
  }
  if (this->full_spectrum_counts_sensor_ != nullptr) {
    this->full_spectrum_counts_sensor_->publish_state(data.ch1);
  }
}

void LTRAlsPs507Component::publish_data_part_2_(AlsReadings &data) {
  if (this->actual_gain_sensor_ != nullptr) {
    this->actual_gain_sensor_->publish_state(get_gain_coeff(data.gain));
  }
  if (this->actual_integration_time_sensor_ != nullptr) {
    this->actual_integration_time_sensor_->publish_state(get_itime_ms(data.integration_time));
  }
}
}  // namespace ltr507
}  // namespace esphome
