#include <string.h>

#include "sbk-qr.h"

/* ---- the tables ----------------------------------------------------------- */

/* Per version (1..16) and level: ECC codewords per block, then two groups of
   (block count, data codewords per block). Straight out of the spec's table 9. */
struct ecc_spec {
	uint8_t ecc, g1, d1, g2, d2;
};

static const struct ecc_spec ECC[16][4] = {
	/*                L                   M                   Q                   H          */
	/*  1 */ {{7, 1, 19, 0, 0},    {10, 1, 16, 0, 0},   {13, 1, 13, 0, 0},   {17, 1, 9, 0, 0}},
	/*  2 */ {{10, 1, 34, 0, 0},   {16, 1, 28, 0, 0},   {22, 1, 22, 0, 0},   {28, 1, 16, 0, 0}},
	/*  3 */ {{15, 1, 55, 0, 0},   {26, 1, 44, 0, 0},   {18, 2, 17, 0, 0},   {22, 2, 13, 0, 0}},
	/*  4 */ {{20, 1, 80, 0, 0},   {18, 2, 32, 0, 0},   {26, 2, 24, 0, 0},   {16, 4, 9, 0, 0}},
	/*  5 */ {{26, 1, 108, 0, 0},  {24, 2, 43, 0, 0},   {18, 2, 15, 2, 16},  {22, 2, 11, 2, 12}},
	/*  6 */ {{18, 2, 68, 0, 0},   {16, 4, 27, 0, 0},   {24, 4, 19, 0, 0},   {28, 4, 15, 0, 0}},
	/*  7 */ {{20, 2, 78, 0, 0},   {18, 4, 31, 0, 0},   {18, 2, 14, 4, 15},  {26, 4, 13, 1, 14}},
	/*  8 */ {{24, 2, 97, 0, 0},   {22, 2, 38, 2, 39},  {22, 4, 18, 2, 19},  {26, 4, 14, 2, 15}},
	/*  9 */ {{30, 2, 116, 0, 0},  {22, 3, 36, 2, 37},  {20, 4, 16, 4, 17},  {24, 4, 12, 4, 13}},
	/* 10 */ {{18, 2, 68, 2, 69},  {26, 4, 43, 1, 44},  {24, 6, 19, 2, 20},  {28, 6, 15, 2, 16}},
	/* 11 */ {{20, 4, 81, 0, 0},   {30, 1, 50, 4, 51},  {28, 4, 22, 4, 23},  {24, 3, 12, 8, 13}},
	/* 12 */ {{24, 2, 92, 2, 93},  {22, 6, 36, 2, 37},  {26, 4, 20, 6, 21},  {28, 7, 14, 4, 15}},
	/* 13 */ {{26, 4, 107, 0, 0},  {22, 8, 37, 1, 38},  {24, 8, 20, 4, 21},  {22, 12, 11, 4, 12}},
	/* 14 */ {{30, 3, 115, 1, 116},{24, 4, 40, 5, 41},  {20, 11, 16, 5, 17}, {24, 11, 12, 5, 13}},
	/* 15 */ {{22, 5, 87, 1, 88},  {24, 5, 41, 5, 42},  {30, 5, 24, 7, 25},  {24, 11, 12, 7, 13}},
	/* 16 */ {{24, 5, 98, 1, 99},  {28, 7, 45, 3, 46},  {24, 15, 19, 2, 20}, {30, 3, 15, 13, 16}},
};

/* alignment-pattern centres per version; 0 terminates */
static const uint8_t ALIGN[16][4] = {
	{0}, {6, 18}, {6, 22}, {6, 26}, {6, 30}, {6, 34},
	{6, 22, 38}, {6, 24, 42}, {6, 26, 46}, {6, 28, 50}, {6, 30, 54},
	{6, 32, 58}, {6, 34, 62}, {6, 26, 46, 66}, {6, 26, 48, 70}, {6, 26, 50, 74},
};

/* the two bits the format information carries for each level */
static const uint8_t ECC_BITS[4] = {1, 0, 3, 2}; /* L, M, Q, H */

/* ---- GF(256) -------------------------------------------------------------- */

static uint8_t GF_EXP[512], GF_LOG[256];

static void gf_init(void)
{
	if (GF_EXP[0])
		return;
	int x = 1;
	for (int i = 0; i < 255; i++) {
		GF_EXP[i] = (uint8_t)x;
		GF_LOG[x] = (uint8_t)i;
		x <<= 1;
		if (x & 0x100)
			x ^= 0x11D; /* the QR field's primitive polynomial */
	}
	for (int i = 255; i < 512; i++)
		GF_EXP[i] = GF_EXP[i - 255];
}

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
	if (!a || !b)
		return 0;
	return GF_EXP[GF_LOG[a] + GF_LOG[b]];
}

/* the generator polynomial for n error-correction codewords */
static void gf_generator(int n, uint8_t *out)
{
	out[0] = 1;
	int len = 1;
	for (int i = 0; i < n; i++) {
		/* multiply by (x - a^i) */
		out[len] = 0;
		for (int j = len; j > 0; j--)
			out[j] = out[j - 1] ^ gf_mul(out[j], GF_EXP[i]);
		out[0] = gf_mul(out[0], GF_EXP[i]);
		len++;
	}
}

static void rs_encode(const uint8_t *data, int data_len, int ecc_len, uint8_t *out)
{
	uint8_t gen[64];
	gf_generator(ecc_len, gen);
	memset(out, 0, (size_t)ecc_len);
	for (int i = 0; i < data_len; i++) {
		uint8_t factor = data[i] ^ out[0];
		memmove(out, out + 1, (size_t)(ecc_len - 1));
		out[ecc_len - 1] = 0;
		for (int j = 0; j < ecc_len; j++)
			out[j] ^= gf_mul(gen[ecc_len - 1 - j], factor);
	}
}

/* ---- BCH, for the format and version fields -------------------------------- */

/* polynomial long division over GF(2): the remainder of value by poly */
static uint32_t bch(uint32_t value, uint32_t poly)
{
	int deg = 0;
	for (uint32_t p = poly; p >>= 1;)
		deg++;
	uint32_t rem = value;
	for (int i = 31; i >= deg; i--)
		if (rem & (1u << i))
			rem ^= poly << (i - deg);
	return rem;
}

/* ---- the matrix ----------------------------------------------------------- */

struct grid {
	int size;
	uint8_t m[SBK_QR_MAX_SIZE * SBK_QR_MAX_SIZE];
	uint8_t fixed[SBK_QR_MAX_SIZE * SBK_QR_MAX_SIZE]; /* function patterns: not data, never masked */
};

static void set(struct grid *g, int x, int y, int v, int fixed)
{
	if (x < 0 || y < 0 || x >= g->size || y >= g->size)
		return;
	g->m[y * g->size + x] = (uint8_t)v;
	if (fixed)
		g->fixed[y * g->size + x] = 1;
}

static int get(const struct grid *g, int x, int y)
{
	if (x < 0 || y < 0 || x >= g->size || y >= g->size)
		return 0;
	return g->m[y * g->size + x];
}

static void finder(struct grid *g, int cx, int cy)
{
	for (int dy = -4; dy <= 4; dy++)
		for (int dx = -4; dx <= 4; dx++) {
			int x = cx + dx, y = cy + dy;
			if (x < 0 || y < 0 || x >= g->size || y >= g->size)
				continue;
			int a = dx < 0 ? -dx : dx, b = dy < 0 ? -dy : dy;
			int d = a > b ? a : b;
			set(g, x, y, (d == 2 || d == 4) ? 0 : 1, 1);
		}
}

static void reserve_format(struct grid *g)
{
	int n = g->size;
	for (int i = 0; i < 9; i++) {
		if (i != 6) {
			set(g, i, 8, get(g, i, 8), 1);
			set(g, 8, i, get(g, 8, i), 1);
		}
	}
	for (int i = 0; i < 8; i++) {
		set(g, n - 1 - i, 8, get(g, n - 1 - i, 8), 1);
		set(g, 8, n - 1 - i, get(g, 8, n - 1 - i), 1);
	}
	set(g, 8, n - 8, 1, 1); /* the module that is always dark */
}

static void build_function_patterns(struct grid *g, int version)
{
	int n = g->size;
	finder(g, 3, 3);
	finder(g, n - 4, 3);
	finder(g, 3, n - 4);

	for (int i = 8; i < n - 8; i++) {
		set(g, i, 6, (i % 2 == 0) ? 1 : 0, 1);
		set(g, 6, i, (i % 2 == 0) ? 1 : 0, 1);
	}

	const uint8_t *a = ALIGN[version - 1];
	int count = 0;
	while (count < 4 && a[count])
		count++;
	for (int i = 0; i < count; i++)
		for (int j = 0; j < count; j++) {
			int cx = a[i], cy = a[j];
			/* the three corners already carry a finder */
			if ((cx <= 8 && cy <= 8) || (cx <= 8 && cy >= n - 9) || (cx >= n - 9 && cy <= 8))
				continue;
			for (int dy = -2; dy <= 2; dy++)
				for (int dx = -2; dx <= 2; dx++) {
					int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
					int d = ax > ay ? ax : ay;
					set(g, cx + dx, cy + dy, d == 1 ? 0 : 1, 1);
				}
		}

	if (version >= 7) {
		uint32_t v = ((uint32_t)version << 12) | bch((uint32_t)version << 12, 0x1F25);
		for (int i = 0; i < 18; i++) {
			int bit = (v >> i) & 1;
			set(g, i / 3, n - 11 + i % 3, bit, 1);
			set(g, n - 11 + i % 3, i / 3, bit, 1);
		}
	}

	reserve_format(g);
}

static bool mask_at(int mask, int x, int y)
{
	switch (mask) {
	case 0: return (x + y) % 2 == 0;
	case 1: return y % 2 == 0;
	case 2: return x % 3 == 0;
	case 3: return (x + y) % 3 == 0;
	case 4: return (y / 2 + x / 3) % 2 == 0;
	case 5: return (x * y) % 2 + (x * y) % 3 == 0;
	case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
	default: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
	}
}

static void write_format(struct grid *g, enum sbk_qr_ecc ecc, int mask)
{
	int n = g->size;
	uint32_t data = (uint32_t)((ECC_BITS[ecc] << 3) | mask);
	uint32_t bits = ((data << 10) | bch(data << 10, 0x537)) ^ 0x5412;
	for (int i = 0; i < 15; i++) {
		uint8_t bit = (uint8_t)((bits >> i) & 1);
		/* the copy around the top-left finder: down column 8, then left
		   along row 8 */
		if (i < 6)
			g->m[i * n + 8] = bit;             /* (8, i) */
		else if (i == 6)
			g->m[7 * n + 8] = bit;             /* (8, 7) — row 6 is timing */
		else if (i == 7)
			g->m[8 * n + 8] = bit;             /* (8, 8) */
		else if (i == 8)
			g->m[8 * n + 7] = bit;             /* (7, 8) */
		else
			g->m[8 * n + (14 - i)] = bit;      /* (14 - i, 8) */
		/* and the split copy: along row 8 from the right, then up column 8
		   from the bottom */
		if (i < 8)
			g->m[8 * n + (n - 1 - i)] = bit;   /* (n-1-i, 8) */
		else
			g->m[(n - 15 + i) * n + 8] = bit;  /* (8, n-15+i) */
	}
	g->m[(n - 8) * n + 8] = 1; /* the module that is always dark */
}

/* the four rules from the spec, summed */
static int penalty(const struct grid *g)
{
	int n = g->size, score = 0, dark = 0;

	for (int y = 0; y < n; y++) {
		for (int x = 0; x < n; x++)
			if (get(g, x, y))
				dark++;
		/* runs of five or more, in both directions */
		for (int dir = 0; dir < 2; dir++) {
			int run = 1, prev = -1;
			for (int i = 0; i < n; i++) {
				int v = dir ? get(g, y, i) : get(g, i, y);
				if (v == prev) {
					run++;
				} else {
					if (run >= 5)
						score += 3 + (run - 5);
					run = 1;
					prev = v;
				}
			}
			if (run >= 5)
				score += 3 + (run - 5);
		}
	}

	for (int y = 0; y < n - 1; y++)
		for (int x = 0; x < n - 1; x++) {
			int v = get(g, x, y);
			if (v == get(g, x + 1, y) && v == get(g, x, y + 1) && v == get(g, x + 1, y + 1))
				score += 3;
		}

	/* 1:1:3:1:1 with four light modules on one side, either way round */
	static const int P1[7] = {1, 0, 1, 1, 1, 0, 1};
	for (int y = 0; y < n; y++)
		for (int x = 0; x < n; x++)
			for (int dir = 0; dir < 2; dir++) {
				if (dir == 0 && x + 7 > n)
					continue;
				if (dir == 1 && y + 7 > n)
					continue;
				bool hit = true;
				for (int k = 0; k < 7; k++) {
					int v = dir ? get(g, x, y + k) : get(g, x + k, y);
					if (v != P1[k]) {
						hit = false;
						break;
					}
				}
				if (!hit)
					continue;
				bool before = true, after = true;
				for (int k = 1; k <= 4; k++) {
					int bx = dir ? x : x - k, by = dir ? y - k : y;
					int ax = dir ? x : x + 7 + k - 1, ay = dir ? y + 7 + k - 1 : y;
					if (bx < 0 || by < 0 || get(g, bx, by))
						before = false;
					if (ax >= n || ay >= n || get(g, ax, ay))
						after = false;
				}
				if (before || after)
					score += 40;
			}

	int total = n * n;
	int pct = dark * 100 / total;
	int k = (pct > 50 ? pct - 50 : 50 - pct) / 5;
	score += k * 10;
	return score;
}

/* ---- the encoder ---------------------------------------------------------- */

bool sbk_qr_encode(struct sbk_qr *qr, const char *text, enum sbk_qr_ecc ecc)
{
	gf_init();
	if (!text)
		text = "";
	size_t len = strlen(text);

	/* the smallest version the text fits in at this level */
	int version = 0, data_cw = 0;
	for (int v = 1; v <= 16; v++) {
		const struct ecc_spec *e = &ECC[v - 1][ecc];
		int cap = e->g1 * e->d1 + e->g2 * e->d2;
		int count_bits = v < 10 ? 8 : 16;
		int need = (4 + count_bits + (int)len * 8 + 7) / 8;
		if (need <= cap) {
			version = v;
			data_cw = cap;
			break;
		}
	}
	if (!version)
		return false;

	const struct ecc_spec *e = &ECC[version - 1][ecc];
	int count_bits = version < 10 ? 8 : 16;

	/* the bit stream */
	uint8_t buf[2048];
	memset(buf, 0, sizeof(buf));
	int bitpos = 0;
#define PUT(value, bits)                                                      \
	do {                                                                  \
		for (int _i = (bits) - 1; _i >= 0; _i--) {                    \
			if (((value) >> _i) & 1)                              \
				buf[bitpos >> 3] |= (uint8_t)(0x80 >> (bitpos & 7)); \
			bitpos++;                                             \
		}                                                             \
	} while (0)
	PUT(4, 4); /* byte mode */
	PUT((int)len, count_bits);
	for (size_t i = 0; i < len; i++)
		PUT((uint8_t)text[i], 8);
	int cap_bits = data_cw * 8;
	int term = cap_bits - bitpos;
	PUT(0, term > 4 ? 4 : (term < 0 ? 0 : term));
	while (bitpos % 8)
		PUT(0, 1);
	/* the two pad bytes, alternating, until the version is full */
	for (int i = 0; bitpos < cap_bits; i++)
		PUT(i % 2 ? 0x11 : 0xEC, 8);
#undef PUT

	/* split into blocks, and give each its own error-correction codewords */
	int blocks = e->g1 + e->g2;
	uint8_t bd[64][128], be[64][64];
	int blen[64];
	int at = 0;
	for (int b = 0; b < blocks; b++) {
		int dlen = b < e->g1 ? e->d1 : e->d2;
		blen[b] = dlen;
		memcpy(bd[b], buf + at, (size_t)dlen);
		at += dlen;
		rs_encode(bd[b], dlen, e->ecc, be[b]);
	}

	/* interleave: one codeword from each block in turn, data then ECC */
	uint8_t out[4096];
	int olen = 0;
	int maxd = e->d1 > e->d2 ? e->d1 : e->d2;
	for (int i = 0; i < maxd; i++)
		for (int b = 0; b < blocks; b++)
			if (i < blen[b])
				out[olen++] = bd[b][i];
	for (int i = 0; i < e->ecc; i++)
		for (int b = 0; b < blocks; b++)
			out[olen++] = be[b][i];

	/* the matrix */
	struct grid g;
	memset(&g, 0, sizeof(g));
	g.size = version * 4 + 17;
	build_function_patterns(&g, version);

	/* data goes in two-module columns, upward then downward, skipping the
	   vertical timing line */
	int n = g.size, bit = 0, total_bits = olen * 8;
	int col = n - 1;
	bool up = true;
	while (col > 0) {
		if (col == 6)
			col--;
		for (int i = 0; i < n; i++) {
			int y = up ? n - 1 - i : i;
			for (int k = 0; k < 2; k++) {
				int x = col - k;
				if (g.fixed[y * n + x])
					continue;
				int v = 0;
				if (bit < total_bits)
					v = (out[bit >> 3] >> (7 - (bit & 7))) & 1;
				bit++;
				g.m[y * n + x] = (uint8_t)v;
			}
		}
		up = !up;
		col -= 2;
	}

	/* try every mask and keep the least offensive */
	struct grid best;
	int best_score = 0;
	for (int mask = 0; mask < 8; mask++) {
		struct grid t = g;
		for (int y = 0; y < n; y++)
			for (int x = 0; x < n; x++)
				if (!t.fixed[y * n + x] && mask_at(mask, x, y))
					t.m[y * n + x] ^= 1;
		write_format(&t, ecc, mask);
		int score = penalty(&t);
		if (mask == 0 || score < best_score) {
			best_score = score;
			best = t;
		}
	}

	qr->size = n;
	memcpy(qr->m, best.m, (size_t)(n * n));
	return true;
}
