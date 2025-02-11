/*
 * ps2kpolled.c
 *
 * Copyright 2009 by Bill Westfield
 *
 * Polled driver for PS2-style keyboards.
 * See http://www.beyondlogic.org/keyboard/keybrd.htm
 */

#include "ps2keypolled.h"

unsigned char ps2k_clk=3, ps2k_dat=7;

void (*cb_func_usr)(int key_code);

#define WAITLOW(pin) while (digitalRead(pin) != 0) ;
#define WAITHIGH(pin) while (digitalRead(pin) != 1) ;

/*
 * ps2k_init
 * initialize our interface to a PS2 keyboard.
 * Set the clock and data pins
 */
void ps2k_init (unsigned char clock, unsigned char data)
{
    ps2k_clk = clock;
    ps2k_dat = data;
    pinMode(ps2k_clk, INPUT);
    digitalWrite(ps2k_clk, 1);		/* Enable pullups */
    pinMode(ps2k_dat, INPUT);
    digitalWrite(ps2k_dat, 1);		/* Enable pullups */
}


/*
 * ps2k_getcode
 * Read a single keycode from a PS2 keyboard.  This may be a leadin
 * to a multi-symbol key, or an acknowledgement, or a key-release
 * event; we don't care at all.
 *
 * This is primarilly an internal subroutine.
 */
unsigned char ps2k_getcode(void)
{
    unsigned char keycode, i;

    WAITLOW(ps2k_clk);			/* Wait for Start bit */
    WAITHIGH(ps2k_clk);			/*  Discard start bit */

    keycode = 0;
    for (i=0; i<8; i++) {		/* Read 8 data bits */
	WAITLOW(ps2k_clk);		/*   On each falling edge of clock */
	keycode >>= 1;			/* LSB is sent first, shift in bits */
	if (digitalRead(ps2k_dat)) {	/*   from the MSB of the result */
	    keycode |= 0x80;
	}
	WAITHIGH(ps2k_clk);		/* prepare for next clock transition */
    }
    WAITLOW(ps2k_clk);			/* get parity bit */
    WAITHIGH(ps2k_clk);			/*  And ignore it! */
    WAITLOW(ps2k_clk);			/* get stop bit */
    WAITHIGH(ps2k_clk);			/*  Don't check it either! */
    return(keycode);
}

/*
 * ps2k_getkey
 * return a "meaningful" keypress event.
 *   returns <keycode> for simple keys
 *   returns <keycode>+PS2K_EXTEND for keys with an E0 leadin code
 *   returns -(<keycode>) for key release events (negative numbers!)
 */
int ps2k_getkey(void)
{
    unsigned char keycode;
    int result;

    result = 0;
    pinMode(ps2k_clk, INPUT);       /* make sure clk is an input. */
    digitalWrite(ps2k_clk, 1);      /* Enable pullups */
    digitalWrite(ps2k_dat, HIGH);	/* make sure data is also an input. */
    pinMode(ps2k_dat, INPUT);		/*   With pullup! */

    result = keycode = ps2k_getcode();	/* Get one byte of keycode */
    if (keycode == 0xF0) {		/* Is it a key release ? */
	keycode = ps2k_getcode();	/*   get next code for which key released*/
	result = - (int) keycode;	/*   and return negative value */
    } else if (keycode == 0xE0) {	/* Is it an extended keycode */
	keycode = ps2k_getcode();	/*   get next byte */
	if (keycode != 0xF0) {		/*   release of extended keycode ? */
	    result = (PS2K_EXTEND + keycode);	/*   nope  */
	} else {
	    keycode = ps2k_getcode();	/* Key being released */
	    result = - (PS2K_EXTEND + keycode);
	}
    }
    return result;
}

/*
 * ps2k_sendbyte
 * Send a byte from the host to the keyboard.
 * This is ... complicated, because even though the host is sending the
 * data, the keyboard does the clocking.  I had all sorts of troubles
 * with terminating the write, apparently solved by the insertion of
 * the "delayMicroseconds()" call after the parity bit.
 * Note that this routine does not process any response codes that the
 * keyboard might send.  They'll show up to ps2k_getkey and will need to
 * be ignorned (or not, as the case may be.)
 */

//TODO : Make sure, this is working...

void ps2k_sendbyte(unsigned char code)
{
    unsigned char parity = 0;		/* Get first byte */
    unsigned char i;

    pinMode(ps2k_dat, OUTPUT);		/* Claim the data line */
    digitalWrite(ps2k_dat, 0);		/* Say we want to send data */

    digitalWrite(ps2k_clk, 1);
    pinMode(ps2k_clk, INPUT);		/* Let the kbd drive the clock */

    WAITLOW(ps2k_clk);			/* Send Start bit */

    for (i = 0; i < 8; i++) {		/* Send 8 data bits, LSB first */
	if (code & 0x1) {
	    digitalWrite(ps2k_dat, 1);
	    parity++;			/* Count one bits to calculate parity */
	} else {
	    digitalWrite(ps2k_dat, 0);
	}
	WAITHIGH(ps2k_clk);
	WAITLOW(ps2k_clk);
	code >>= 1;			/* Shift to get next bit ready. */
    }

    if ((parity & 0x1)) {		/* Send proper parity bit */
	digitalWrite(ps2k_dat, 0);
    } else {
	digitalWrite(ps2k_dat, 1);
    }
    WAITHIGH(ps2k_clk);
    WAITLOW(ps2k_clk);

    pinMode(ps2k_dat, INPUT);		/* Release data line for "ACK" */
    digitalWrite(ps2k_dat, HIGH);
    delayMicroseconds(50);		/* This seems to be important! */
    while (digitalRead(ps2k_clk) == 0 || digitalRead(ps2k_dat) == 0)
	;  				/* Wait for keyboard to be done */
}

void cb_func_clk() {
    int key = ps2k_getkey();
    cb_func_usr(key);
}

void ps2k_attach(void (*callback_function)(int key_code)) {
    cb_func_usr = callback_function;
    attachInterrupt(digitalPinToInterrupt(ps2k_clk), cb_func_clk, LOW);
}