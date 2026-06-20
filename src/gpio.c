#include "gpio.h"

void gpio_enable_all_clocks(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN
                  | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN
                  | RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOHEN;
    (void)RCC->AHB1ENR; /* short delay after enabling clock */
}

void gpio_config(GPIO_TypeDef *g, int pin, int mode, int otype, int speed, int pupd, int af) {
    g->MODER   = (g->MODER   & ~(3u << (pin * 2))) | ((uint32_t)mode  << (pin * 2));
    g->OTYPER  = (g->OTYPER  & ~(1u <<  pin))      | ((uint32_t)otype <<  pin);
    g->OSPEEDR = (g->OSPEEDR & ~(3u << (pin * 2))) | ((uint32_t)speed << (pin * 2));
    g->PUPDR   = (g->PUPDR   & ~(3u << (pin * 2))) | ((uint32_t)pupd  << (pin * 2));
    int idx = pin >> 3;
    int off = (pin & 7) * 4;
    g->AFR[idx] = (g->AFR[idx] & ~(0xFu << off)) | ((uint32_t)af << off);
}
