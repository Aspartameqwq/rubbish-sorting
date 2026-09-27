#ifndef PITCH_AXIS_H
#define PITCH_AXIS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PITCH_AXIS_STATUS_OK = 0,
    PITCH_AXIS_STATUS_INVALID_ARGUMENT,
    PITCH_AXIS_STATUS_NOT_INITIALIZED,
    PITCH_AXIS_STATUS_LIMIT,
    PITCH_AXIS_STATUS_DRIVER_ERROR
} PitchAxisStatus;

PitchAxisStatus PitchAxis_Init(void);
PitchAxisStatus PitchAxis_ValidateTargetMilliDeg(int32_t target_mdeg);
PitchAxisStatus PitchAxis_SetTargetMilliDeg(int32_t target_mdeg);
/* Raw bounded pulse control is for calibration/bench use and invalidates angle telemetry. */
PitchAxisStatus PitchAxis_SetRawPulseUs(uint32_t pulse_us);

int32_t PitchAxis_GetTargetMilliDeg(void);
int32_t PitchAxis_GetCommandedMilliDeg(void);
int32_t PitchAxis_GetMeasuredMilliDeg(void);
bool PitchAxis_IsMeasurementValid(void);
bool PitchAxis_IsCommandedAngleValid(void);
bool PitchAxis_IsRawPulseMode(void);
bool PitchAxis_IsServoEnabled(void);
bool PitchAxis_IsCalibrationValid(void);
bool PitchAxis_IsSoftLimitEnabled(void);
int32_t PitchAxis_GetSoftLimitMinMilliDeg(void);
int32_t PitchAxis_GetSoftLimitMaxMilliDeg(void);
uint32_t PitchAxis_GetLimitRejectCount(void);
PitchAxisStatus PitchAxis_GetLastStatus(void);
uint32_t PitchAxis_GetPulseUs(void);

#endif /* PITCH_AXIS_H */
