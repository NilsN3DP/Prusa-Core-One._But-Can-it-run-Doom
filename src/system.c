/* Clock + timing for STM32F427 @ 168 MHz (HSE 12 MHz), standalone DOOM firmware.
 * Mirrors Prusa's xBuddy clock tree: PLLM6/N168/P2/Q7, flash 5 WS, VOS1, APB1/4 APB2/2.
 *
 * Hardened for chain-loading by the Prusa bootloader: relocate VTOR, disable all
 * peripheral IRQs the bootloader may have left enabled, and use an interrupt-free
 * DWT-cycle-counter delay so display bring-up can't hang on a stalled SysTick. */

#include "stm32f4xx.h"

uint32_t SystemCoreClock = 168000000u;

void SystemCoreClockUpdate(void) { SystemCoreClock = 168000000u; }

/* Optional 1 kHz callback (the Doom build polls the rotary encoder here, far
 * faster than the ~35 Hz game loop, so quadrature transitions aren't missed). */
void (*g_systick_hook)(void) = 0;

void SysTick_Handler(void) {
    if (g_systick_hook) {
        g_systick_hook();
    }
}

/* Monotonic millisecond counter derived from the DWT cycle counter (which is
 * known-good from the bring-up). Wrap-safe as long as it is polled more often
 * than the ~25.6 s CYCCNT wrap (the Doom loop calls it many times/second).
 * Not reentrant — called only from the single-threaded game loop. */
uint32_t millis(void) {
    static uint32_t last_cyc = 0, ms = 0, rem = 0;
    uint32_t now = DWT->CYCCNT;
    uint32_t diff = now - last_cyc; /* unsigned: correct across 32-bit wrap */
    last_cyc = now;
    uint32_t cyc_per_ms = SystemCoreClock / 1000u; /* 168000 */
    uint32_t total = diff + rem;
    ms += total / cyc_per_ms;
    rem = total % cyc_per_ms;
    return ms;
}

/* ---- interrupt-free delays via the DWT cycle counter (168 MHz) ---- */
void delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < ticks) { }
}

void delay_ms(uint32_t ms) {
    while (ms--) {
        delay_us(1000);
    }
}

#ifndef VECT_TAB_BASE
    #define VECT_TAB_BASE 0x08000000u
#endif

static void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void SystemInit(void) {
    /* Relocate the vector table to our image base (we may be at 0x08020200). */
    SCB->VTOR = VECT_TAB_BASE;

    /* The bootloader may leave peripheral IRQs enabled/pending. Disable & clear
     * them all so a stray IRQ can't vector into a half-initialised handler. */
    __disable_irq();
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    /* Enable FPU (CP10/CP11 full access) — required for -mfloat-abi=hard */
    SCB->CPACR |= ((3u << 10 * 2) | (3u << 11 * 2));

    /* Reset RCC to a known state, switching SYSCLK to HSI first. */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }
    RCC->CFGR = 0x00000000u;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) { } /* wait until HSI is the source */
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010u; /* reset value */
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0x00000000u;

    /* Power interface: enable clock and select voltage scale 1 (for 168 MHz) */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    /* Start HSE (12 MHz crystal) */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { }

    /* Flash: prefetch + I/D cache + 5 wait states */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

    /* Bus prescalers: AHB /1 (168 MHz), APB1 /4 (42 MHz), APB2 /2 (84 MHz) */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* PLL: HSE, M=6, N=168, P=2, Q=7 → VCO 336 MHz, SYSCLK 168 MHz, USB 48 MHz */
    RCC->PLLCFGR =
        (6u)
        | (168u << 6)
        | (((2u / 2u) - 1u) << 16)
        | RCC_PLLCFGR_PLLSRC_HSE
        | (7u << 24);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    SystemCoreClock = 168000000u;

    dwt_init();                              /* interrupt-free delays */
    SysTick_Config(SystemCoreClock / 40000u); /* 40 kHz: encoder polling + stepper-music DDS */
    __enable_irq();
}
