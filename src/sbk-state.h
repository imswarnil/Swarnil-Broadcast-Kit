#pragma once

#include <stdbool.h>
#include <stdint.h>

/*  What the program is doing — the truth the tally light shows. Fed by the
    frontend's events, read by any source on the graphics thread; three bools
    written from the UI thread are fine to read racily for a lamp.  */

struct sbk_status {
	bool streaming, recording, paused, ready;
	/* when each started, for the uptime the stats panel shows */
	uint64_t stream_started_ns, record_started_ns;
};

extern struct sbk_status sbk_status;

void sbk_state_init(void);

/* one word for the light: off | live | rec | both */
const char *sbk_state_word(void);

/* seconds since streaming (or recording) started, 0 when it has not */
double sbk_state_uptime(bool recording);
