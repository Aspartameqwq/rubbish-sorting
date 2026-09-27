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
    PITCH_AXIS_STATUS_DISABLED,
    PITCH_AXIS_STATUS_DRIVER_ERROR
} PitchAxisStatus;

PitchAxisStatus PitchAxis_Init(void);
PitchAxisStatus PitchAxis_ValidateTargetMilliDeg(int32_t target_mdeg);
PitchAxisStatus PitchAxis_SetTargetMilliDeg(int32_t target_mdeg);
/* Raw bench calls remain hard-limit checked and are compiled out of Release builds. */
PitchAxisStatus PitchAxis_SetRawPulseUs(uint32_t pulse_us);
PitchAxisStatus PitchAxis_SetRawServoAngleMilliDeg(int32_t servo_angle_mdeg);
PitchAxisStatus PitchAxis_SetResponseTimeMs(uint32_t response_time_ms);
void PitchAxis_Process(uint32_t now_ms);

int32_t PitchAxis_GetTargetMilliDeg(void);
int32_t PitchAxis_GetCommandedMilliDeg(void);
int32_t PitchAxis_GetServoTargetMilliDeg(void);
int32_t PitchAxis_GetMeasuredMilliDeg(void);
uint32_t PitchAxis_GetResponseTimeMs(void);
uint32_t PitchAxis_GetActiveResponseTimeMs(void);
uint32_t PitchAxis_GetTrajectoryElapsedMs(void);
bool PitchAxis_IsMoving(void);
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
