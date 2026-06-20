/* Rotary encoder + button (the printer knob) on xBuddy. */
#pragma once

void input_init(void);
void input_poll(void);        /* call frequently to track quadrature */
int  input_take_delta(void);  /* signed encoder steps (per detent) since last call */
int  input_take_raw(void);    /* signed raw quadrature transitions since last call */
int  input_button_down(void); /* 1 while button held (active low) */
int  input_raw_a(void);       /* live level of encoder line A */
int  input_raw_b(void);       /* live level of encoder line B */
