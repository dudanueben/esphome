#pragma once

#include <cstdint>

namespace esphome {
namespace ltr507 {

enum class CommandRegisters : uint8_t {
  ALS_CONTR = 0x80,          // ALS operation mode control and SW reset
  PS_CONTR = 0x81,           // PS operation mode control
  PS_LED = 0x82,             // PS LED pulse frequency control
  PS_N_PULSES = 0x83,        // PS number of pulses control
  PS_MEAS_RATE = 0x84,       // PS measurement rate in active mode
  MEAS_RATE = 0x85,          // ALS measurement rate in active mode
  PART_ID = 0x86,            // Part Number ID and Revision ID
  MANUFAC_ID = 0x87,         // Manufacturer ID
  ALS_DATA_0 = 0x88,         // Direct ALS measurement, lower byte
  ALS_DATA_1 = 0x89,         // Direct ALS measurement, upper byte
  ALS_PS_STATUS = 0x8A,      // ALS PS new data status
  PS_DATA_0 = 0x8B,          // PS measurement data, lower byte
  PS_DATA_1 = 0x8C,          // PS measurement data, upper byte
  ALS_DATA_CH1_0 = 0x8D,     // ALS measurement CH1 data, lower byte - visible + infrared
  ALS_DATA_CH1_1 = 0x8E,     // ALS measurement CH1 data, middle byte - visible + infrared
  ALS_DATA_CH1_2 = 0x8F,     // ALS measurement CH1 data, upper byte - visible + infrared
  ALS_DATA_CH2_0 = 0x90,     // ALS measurement CH2 data, lower byte - infrared only
  ALS_DATA_CH2_1 = 0x91,     // ALS measurement CH2 data, middle byte - infrared only
  ALS_DATA_CH2_2 = 0x92,     // ALS measurement CH2 data, upper byte - infrared only
  ALS_COEFF1_DATA_0 = 0x93,  // Coefficient for Clear diode, lower byte
  ALS_COEFF1_DATA_1 = 0x94,  // Coefficient for Clear diode, upper byte
  ALS_COEFF2_DATA_0 = 0x95,  // Coefficient for IR diode, lower byte
  ALS_COEFF2_DATA_1 = 0x96,  // Coefficient for IR diode, lower byte
  ALS_IRF_CUT_OFF = 0x97,    // ALS cut-off limit of IR factor
  ALS_PS_INTERRUPT = 0x98,   // Interrupt settings
  PS_THRES_UP_0 = 0x99,      // PS interrupt upper threshold, lower byte
  PS_THRES_UP_1 = 0x9A,      // PS interrupt upper threshold, upper byte
  PS_THRES_LOW_0 = 0x9B,     // PS interrupt lower threshold, lower byte
  PS_THRES_LOW_1 = 0x9C,     // PS interrupt lower threshold, upper byte
                             // 0x9D - reserved
  ALS_THRES_UP_0 = 0x9E,     // ALS interrupt upper threshold, lower byte
  ALS_THRES_UP_1 = 0x9F,     // ALS interrupt upper threshold, upper byte
  ALS_THRES_LOW_0 = 0xA0,    // ALS interrupt lower threshold, lower byte
  ALS_THRES_LOW_1 = 0xA1,    // ALS interrupt lower threshold, upper byte
  INTERRUPT_PERSIST = 0xA4,  // ALS/PS Interrupt persist setting
};

// ALS Sensor gain levels
enum AlsGain507 : uint8_t {
  GAIN_1 = 0,    // GAIN_RANGE_1 // default
  GAIN_2 = 1,    // GAIN_RANGE_2
  GAIN_100 = 2,  // GAIN_RANGE_3
  GAIN_200 = 3,  // GAIN_RANGE_4
};
static const uint8_t GAINS_COUNT = 4;

// ALS Sensor integration times
enum IntegrationTime507 : uint8_t {
  INTEGRATION_TIME_1200MS = 0,
  INTEGRATION_TIME_600MS = 1,
  INTEGRATION_TIME_300MS = 2,
  INTEGRATION_TIME_150MS = 3,
  INTEGRATION_TIME_75MS = 4,  // default
  INTEGRATION_TIME_5MS = 5,   // 4.685ms
  INTEGRATION_TIME_292US = 6,
  INTEGRATION_TIME_18US = 7,
};
static const uint8_t TIMES_COUNT = 8;

// ALS Sensor measurement repeat rate
enum MeasurementRepeatRate507 {
  REPEAT_RATE_100MS = 0,
  REPEAT_RATE_200MS = 1,
  REPEAT_RATE_500MS = 2,  // default
  REPEAT_RATE_1000MS = 3,
  REPEAT_RATE_2000MS = 4,
  REPEAT_RATE_2000MS1 = 5,
  REPEAT_RATE_2000MS2 = 6,
  REPEAT_RATE_2000MS3 = 7,

};

// PS Sensor gain levels
enum PsGain507 : uint8_t {
  PS_GAIN_1 = 3,  // default
};

// LED Pulse Modulation Frequency
enum PsLedFreq : uint8_t {
  PS_LED_FREQ_30KHZ = 0,
  PS_LED_FREQ_40KHZ = 1,
  PS_LED_FREQ_50KHZ = 2,
  PS_LED_FREQ_60KHZ = 3,  // default
  PS_LED_FREQ_70KHZ = 4,
  PS_LED_FREQ_80KHZ = 5,
  PS_LED_FREQ_90KHZ = 6,
  PS_LED_FREQ_100KHZ = 7,
};

// LED current duty
enum PsLedDuty : uint8_t {
  PS_LED_DUTY_25 = 0,
  PS_LED_DUTY_50 = 1,  // default
  PS_LED_DUTY_75 = 2,
  PS_LED_DUTY_100 = 3,
};

// LED pulsed current level
enum PsLedCurrent : uint8_t {
  PS_LED_CURRENT_5MA = 0,
  PS_LED_CURRENT_10MA = 1,
  PS_LED_CURRENT_20MA = 2,
  PS_LED_CURRENT_50MA = 3,  // default
  PS_LED_CURRENT_100MA = 4,
  PS_LED_CURRENT_100MA1 = 5,
  PS_LED_CURRENT_100MA2 = 6,
  PS_LED_CURRENT_100MA3 = 7,
};

// PS measurement rate
enum PsMeasurementRate507 : uint8_t {
  PS_MEAS_RATE_12MS = 0,  // 12.5ms, disables ALS
  PS_MEAS_RATE_50MS = 1,
  PS_MEAS_RATE_70MS = 2,
  PS_MEAS_RATE_100MS = 3,  // default
  PS_MEAS_RATE_200MS = 4,
  PS_MEAS_RATE_500MS = 5,
  PS_MEAS_RATE_1000MS = 6,
  PS_MEAS_RATE_2000MS = 7,
};

// Interrupt source
enum InterruptSource507 : uint8_t {
  NO_INTERRUPT = 0,
  PS_INTERRUPT = 1,
  ALS_INTERRUPT = 2,
  // 3 reserved
};

// Interrupt mode
enum InterruptMode507 : uint8_t {
  INTERRUPT_OFF = 0,   // INT output pin 2 is inactive (default)
  INTERRUPT_PS = 1,    // Only PS measurement can trigger interrupt
  INTERRUPT_ALS = 2,   // Only ALS measurement can trigger interrupt
  INTERRUPT_BOTH = 3,  // Both ALS and PS measurement can trigger interrupt
};

//
// ALS_CONTR Register (0x80)
//
union AlsControlRegister507 {
  uint8_t raw;
  struct {
    bool reserved : 1;
    bool als_mode_active : 1;
    bool sw_reset : 1;
    AlsGain507 gain : 2;
    uint8_t reserved_1 : 3;
  } __attribute__((packed));
};

//
// PS_CONTR Register (0x81)
//
union PsControlRegister507 {
  uint8_t raw;
  struct {
    bool reserved_2 : 1;
    bool ps_mode_active : 1;
    PsGain507 ps_gain : 2;
    bool reserved_4 : 1;
    bool reserved_5 : 1;
    bool reserved_6 : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

//
// PS_LED Register (0x82)
//
union PsLedRegister {
  uint8_t raw;
  struct {
    PsLedCurrent ps_led_current : 3;
    PsLedDuty ps_led_duty : 2;
    PsLedFreq ps_led_freq : 3;
  } __attribute__((packed));
};

//
// PS_N_PULSES Register (0x83)
//
union PsNPulsesRegister507 {
  uint8_t raw;
  uint8_t number_of_pulses;
};

//
// PS_MEAS_RATE Register (0x84)
//
union PsMeasurementRateRegister {
  uint8_t raw;
  struct {
    PsMeasurementRate507 ps_measurement_rate : 3;
    uint8_t reserved : 5;
  } __attribute__((packed));
};

//
// ALS_MEAS_RATE Register (0x85)
//
union MeasurementRateRegister507 {
  uint8_t raw;
  struct {
    MeasurementRepeatRate507 measurement_repeat_rate : 3;
    bool reserved_3 : 1;
    bool reserved_4 : 1;
    IntegrationTime507 integration_time : 3;
  } __attribute__((packed));
};

//
// PART_ID Register (0x86) (Read Only)
//
union PartIdRegister {
  uint8_t raw;
  struct {
    uint8_t revision_id : 4;
    uint8_t part_number_id : 4;
  } __attribute__((packed));
};

//
// ALS_PS_STATUS Register (0x8A) (Read Only)
//
union AlsPsStatusRegister {
  uint8_t raw;
  struct {
    bool ps_new_data : 1;                     // 0 - old data, 1 - new data
    bool ps_interrupt : 1;                    // 0 - interrupt signal not active, 1 - interrupt signal active
    bool als_new_data : 1;                    // 0 - old data, 1 - new data
    bool als_interrupt : 1;                   // 0 - interrupt signal not active, 1 - interrupt signal active
    InterruptSource507 interrupt_source : 2;  // interrupt source
    bool reserved_6 : 1;
    bool reserved_7 : 1;
  } __attribute__((packed));
};

//
// PS_DATA_1 Register (0x8C) (Read Only)
//
union PsData1Register {
  uint8_t raw;
  struct {
    uint8_t ps_data_high : 3;
    bool reserved : 1;
    bool ps_valid_flag : 1;  // 0 - valid PS data, 1 - overflow PS data
    uint8_t reserved_2 : 3;
  } __attribute__((packed));
};

//
// ALS_DATA_CHX_0 (0x8D / 0x90) (Read Only)
//
union AlsDataChX0Register {
  uint8_t raw;
  struct {
    uint8_t reserved : 4;
    uint8_t als_data_chx_0 : 4;
  } __attribute__((packed));
};

//
// PS_THRES_X_1 (0x9A / 0x9C)
//
union PsThresX1Register {
  uint8_t raw;
  struct {
    uint8_t ps_x_threshold_low : 3;
    uint8_t reserved : 5;
  } __attribute__((packed));
};

//
// INTERRUPT Register (0x98) (Read Only)
//
union InterruptRegister {
  uint8_t raw;
  struct {
    InterruptMode507 interrupt_mode : 2;
    bool interrupt_polarity : 1;     // 0 - active low (default), 1 - active high
    bool interrupt_output_mode : 1;  // 0 - persist till read, 1 - updated with measurement (default)
    uint8_t reserved : 4;
  } __attribute__((packed));
};

//
// INTERRUPT_PERSIST Register (0x9E)
//
union InterruptPersistRegister {
  uint8_t raw;
  struct {
    uint8_t als_persist : 4;  // 0 - every ALS cycle, 1 - every 2 ALS cycles, ... 15 - every 16 ALS cycles
    uint8_t ps_persist : 4;   // 0 - every PS cycle, 1 - every 2 PS cycles, ... 15 - every 16 PS cycles
  } __attribute__((packed));
};

}  // namespace ltr507
}  // namespace esphome
