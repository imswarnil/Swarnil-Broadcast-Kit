#pragma once

#include <stdbool.h>
#include <stdint.h>

/*  A QR encoder — byte mode, versions 1 to 16, all four error-correction
    levels. Written here rather than pulled in because the build has no
    download step and the whole plugin links nothing but libobs, curl and
    jansson.

    Sixteen versions is 1046 bytes at level M, which is a very long URL. A
    donate link, a membership page or a channel address is nowhere near it.  */

#define SBK_QR_MAX_SIZE 81 /* version 16 is 81 modules across */

enum sbk_qr_ecc { SBK_QR_L = 0, SBK_QR_M, SBK_QR_Q, SBK_QR_H };

struct sbk_qr {
	int size;                                     /* modules across */
	uint8_t m[SBK_QR_MAX_SIZE * SBK_QR_MAX_SIZE]; /* 1 = dark */
};

/* Returns false if the text does not fit in version 16 at that level. */
bool sbk_qr_encode(struct sbk_qr *qr, const char *text, enum sbk_qr_ecc ecc);

static inline bool sbk_qr_at(const struct sbk_qr *qr, int x, int y)
{
	if (x < 0 || y < 0 || x >= qr->size || y >= qr->size)
		return false;
	return qr->m[y * qr->size + x] != 0;
}
