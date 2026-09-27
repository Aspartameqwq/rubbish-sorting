#ifndef YAW_AXIS_H
#define YAW_AXIS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    YAW_AXIS_STATUS_OK = 0,
    YAW_AXIS_STATUS_INVALID_ARGUMENT,
    YAW_AXIS_STATUS_NOT_INITIALIZED,
    YAW_AXIS_STATUS_NOT_REFERENCED,
    YAW_AXIS_STATUS_LIMIT,
    YAW_AXIS_STATUS_BUSY,
    YAW_AXIS_STATUS_DISABLED,
    YAW_AXIS_STATUS_DRIVER_ERROR,
    YAW_AXIS_STATUS_INVALID_STATE
} YawAxisStatus;

typedef enum
{
    YAW_REFERENCE_INVALID = 0,
    YAW_REFERENCE_MANUAL,
    YAW_REFERENCE_HOMED,
    YAW_REFERENCE_SENSOR
} YawReferenceState;

YawAxisStatus YawAxis_Init(void);
YawAxisStatus YawAxis_Enable(void);
YawAxisStatus YawAxis_Disable(void);
YawAxisStatus YawAxis_ValidateTargetMilliDeg(int32_t target_mdeg,
                                              uint32_t pulse_frequency_hz);
YawAxisStatus YawAxis_SetTargetMilliDeg(int32_t target_mdeg,
                                        uint32_t pulse_frequency_hz);
YawAxisStatus YawAxis_MoveRelativePulses(int32_t delta_pulses,
                                         uint32_t pulse_frequency_hz);
YawAxisStatus YawAxis_SetCurrentPositionAsZero(void);
YawAxisStatus YawAxis_Stop(void);
void YawAxis_Process(void);

int32_t YawAxis_GetTargetMilliDeg(void);
int32_t YawAxis_GetQuantizedTargetMilliDeg(void);
int32_t YawAxis_GetCommandedMilliDeg(void);
int32_t YawAxis_GetMeasuredMilliDeg(void);
int32_t YawAxis_GetCommandedPositionPulses(void);
int32_t YawAxis_GetZeroOffsetPulses(void);
uint32_t YawAxis_GetRemainingPulses(void);
uint32_t YawAxis_GetPulseFrequencyHz(void);
uint32_t YawAxis_GetStepperState(void);
bool YawAxis_IsMeasurementValid(void);
bool YawAxis_IsBusy(void);
bool YawAxis_IsEnabled(void);
YawReferenceState YawAxis_GetReferenceState(void);
/* Deprecated compatibility query; mandatory cable limits always return true. */
bool YawAxis_IsSoftLimitEnabled(void);
/* Deprecated compatibility getters; return the mandatory cable angle bounds. */
int32_t YawAxis_GetSoftLimitMinMilliDeg(void);
int32_t YawAxis_GetSoftLimitMaxMilliDeg(void);
int32_t YawAxis_GetCableLimitMinPulses(void);
int32_t YawAxis_GetCableLimitMaxPulses(void);
int32_t YawAxis_GetCableMarginToMinMilliDeg(void);
int32_t YawAxis_GetCableMarginToMaxMilliDeg(void);
uint32_t YawAxis_GetCableRemainingNegativePulses(void);
uint32_t YawAxis_GetCableRemainingPositivePulses(void);
uint32_t YawAxis_GetLimitRejectCount(void);
YawAxisStatus YawAxis_GetLastStatus(void);

#endif /* YAW_AXIS_H */
