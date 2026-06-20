/* C implementation of FixedMul (GBADoom ships it as arm7tdmi ASM in fixeddiv.s,
 * which can't assemble for Cortex-M4 Thumb). Cortex-M4 has a fast 64-bit result
 * multiply, so this compiles to a couple of instructions. */

#include <stdint.h>
#include "m_fixed.h"

fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FRACBITS);
}
