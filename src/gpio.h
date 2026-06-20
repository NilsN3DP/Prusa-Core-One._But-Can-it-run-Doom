/* Minimal GPIO helpers for STM32F4 (CMSIS register level). */
#pragma once
#include "stm32f4xx.h"

enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 };
enum { GPIO_PP = 0, GPIO_OD = 1 };
enum { GPIO_SPD_LOW = 0, GPIO_SPD_MED = 1, GPIO_SPD_HIGH = 2, GPIO_SPD_VHIGH = 3 };
enum { GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_PULLDOWN = 2 };

void gpio_enable_all_clocks(void);
void gpio_config(GPIO_TypeDef *g, int pin, int mode, int otype, int speed, int pupd, int af);

static inline void gpio_set(GPIO_TypeDef *g, int pin)   { g->BSRR = (1u << pin); }
static inline void gpio_clear(GPIO_TypeDef *g, int pin) { g->BSRR = (1u << (pin + 16)); }
static inline void gpio_write(GPIO_TypeDef *g, int pin, int v) { if (v) gpio_set(g, pin); else gpio_clear(g, pin); }
static inline int  gpio_read(GPIO_TypeDef *g, int pin)  { return (g->IDR >> pin) & 1u; }
