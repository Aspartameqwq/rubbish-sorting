#ifndef TEST_TIM_H
#define TEST_TIM_H

#include "main.h"

typedef struct
{
    uint32_t marker;
} TIM_TypeDef;

extern TIM_TypeDef test_tim2_instance;
extern TIM_TypeDef test_tim3_instance;
#define TIM2 (&test_tim2_instance)
#define TIM3 (&test_tim3_instance)

typedef struct
{
    uint32_t Prescaler;
    uint32_t Period;
    uint32_t CounterMode;
} TIM_InitTypeDef;

typedef struct
{
    TIM_TypeDef *Instance;
    TIM_InitTypeDef Init;
    uint32_t compare1;
    uint32_t autoreload;
    uint32_t counter;
} TIM_HandleTypeDef;

typedef struct
{
    uint32_t OCMode;
    uint32_t Pulse;
    uint32_t OCPolarity;
    uint32_t OCFastMode;
} TIM_OC_InitTypeDef;

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

#define TIM_CHANNEL_1 0x00000000U
#define TIM_COUNTERMODE_UP 0x00000000U
#define TIM_OCMODE_PWM1 0x00000006U
#define TIM_OCPOLARITY_HIGH 0x00000000U
#define TIM_OCPOLARITY_LOW 0x00000002U
#define TIM_OCFAST_DISABLE 0x00000000U
#define TIM_FLAG_UPDATE 0x00000001U
#define TIM_FLAG_CC1 0x00000002U

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

void TestTim_SetCompare(TIM_HandleTypeDef *timer, uint32_t channel, uint32_t value);
void TestTim_SetAutoReload(TIM_HandleTypeDef *timer, uint32_t value);
void TestTim_SetCounter(TIM_HandleTypeDef *timer, uint32_t value);
void TestTim_ClearFlag(TIM_HandleTypeDef *timer, uint32_t flags);

#define __HAL_TIM_SET_COMPARE(timer, channel, value) \
    TestTim_SetCompare((timer), (channel), (value))
#define __HAL_TIM_SET_AUTORELOAD(timer, value) TestTim_SetAutoReload((timer), (value))
#define __HAL_TIM_SET_COUNTER(timer, value) TestTim_SetCounter((timer), (value))
#define __HAL_TIM_CLEAR_FLAG(timer, flags) TestTim_ClearFlag((timer), (flags))

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Start_IT(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop_IT(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer,
                                             TIM_OC_InitTypeDef *config,
                                             uint32_t channel);
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *timer);

#endif /* TEST_TIM_H */
