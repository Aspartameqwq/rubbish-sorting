#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include <stdint.h>

typedef struct
{
    uint32_t marker;
} GPIO_TypeDef;

typedef enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET = 1
} GPIO_PinState;

extern GPIO_TypeDef test_gpio_a;
extern GPIO_TypeDef test_gpio_b;
#define GPIOA (&test_gpio_a)
#define GPIOB (&test_gpio_b)

#define GPIO_PIN_0 ((uint16_t)0x0001U)
#define GPIO_PIN_6 ((uint16_t)0x0040U)
#define GPIO_PIN_9 ((uint16_t)0x0200U)
#define GPIO_PIN_10 ((uint16_t)0x0400U)
#define GPIO_PIN_12 ((uint16_t)0x1000U)
#define GPIO_PIN_13 ((uint16_t)0x2000U)

#define TB6600_DIR_Pin GPIO_PIN_12
#define TB6600_DIR_GPIO_Port GPIOB
#define TB6600_ENA_Pin GPIO_PIN_13
#define TB6600_ENA_GPIO_Port GPIOB

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
uint32_t HAL_GetTick(void);

#endif /* TEST_MAIN_H */
