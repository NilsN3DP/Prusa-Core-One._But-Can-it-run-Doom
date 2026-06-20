/* Minimal Cortex-M4 startup for STM32F427VI (standalone DOOM firmware).
 * Vector table + reset handler + default handler. */

#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;
extern uint32_t _sbss, _ebss;
extern uint32_t _siccmram, _sccmram, _eccmram;

extern int main(void);
extern void SystemInit(void);
extern void __libc_init_array(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Core exception handlers — weak, overridable */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void MemManage_Handler(void) WEAK_ALIAS;
void BusFault_Handler(void) WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void DebugMon_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;

/* Full vector table: 16 core vectors + 98 peripheral IRQ slots, all pointing at
 * Default_Handler. A complete table matters when chain-loaded by the Prusa
 * bootloader — if a stray peripheral IRQ fires, it lands on a safe handler
 * instead of vectoring into garbage past a short table. */
#define IRQ16 Default_Handler, Default_Handler, Default_Handler, Default_Handler, \
              Default_Handler, Default_Handler, Default_Handler, Default_Handler, \
              Default_Handler, Default_Handler, Default_Handler, Default_Handler, \
              Default_Handler, Default_Handler, Default_Handler, Default_Handler

__attribute__((section(".isr_vector"), used))
void (* const g_vectors[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    /* 98 peripheral IRQ vectors (STM32F427 has ~91) */
    IRQ16, IRQ16, IRQ16, IRQ16, IRQ16, IRQ16, /* 96 */
    Default_Handler, Default_Handler,         /* 98 */
};

void Reset_Handler(void) {
    /* Copy .data from flash to SRAM */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }
    /* Copy .ccmram initialized data from flash to CCM */
    src = &_siccmram;
    for (uint32_t *dst = &_sccmram; dst < &_eccmram; ) {
        *dst++ = *src++;
    }
    /* Zero .bss */
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }

    SystemInit();
    __libc_init_array();
    main();

    for (;;) { /* main should not return */ }
}

void Default_Handler(void) {
    for (;;) { __asm__ volatile("bkpt #0"); }
}

/* __libc_init_array calls these; crti.o/crtn.o are excluded by -nostartfiles. */
__attribute__((used)) void _init(void) {}
__attribute__((used)) void _fini(void) {}
