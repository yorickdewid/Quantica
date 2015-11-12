#include <config.h>
#include <common.h>

#include "zmalloc.h"
#include "json_check.h"

enum classes {
	C_SPACE,  /* space */
	C_WHITE,  /* other whitespace */
	C_LCURB,  /* {  */
	C_RCURB,  /* } */
	C_LSQRB,  /* [ */
	C_RSQRB,  /* ] */
	C_COLON,  /* : */
	C_COMMA,  /* , */
	C_QUOTE,  /* " */
	C_BACKS,  /* \ */
	C_SLASH,  /* / */
	C_PLUS,   /* + */
	C_MINUS,  /* - */
	C_POINT,  /* . */
	C_ZERO ,  /* 0 */
	C_DIGIT,  /* 123456789 */
	C_LOW_A,  /* a */
	C_LOW_B,  /* b */
	C_LOW_C,  /* c */
	C_LOW_D,  /* d */
	C_LOW_E,  /* e */
	C_LOW_F,  /* f */
	C_LOW_L,  /* l */
	C_LOW_N,  /* n */
	C_LOW_R,  /* r */
	C_LOW_S,  /* s */
	C_LOW_T,  /* t */
	C_LOW_U,  /* u */
	C_ABCDF,  /* ABCDF */
	C_E,      /* E */
	C_ETC,    /* everything else */
	NR_CLASSES
};

enum states {
	GO,  /* start    */
	OK,  /* ok       */
	OB,  /* object   */
	KE,  /* key      */
	CO,  /* colon    */
	VA,  /* value    */
	AR,  /* array    */
	ST,  /* string   */
	ES,  /* escape   */
	U1,  /* u1       */
	U2,  /* u2       */
	U3,  /* u3       */
	U4,  /* u4       */
	MI,  /* minus    */
	ZE,  /* zero     */
	IN,  /* integer  */
	FR,  /* fraction */
	E1,  /* e        */
	E2,  /* ex       */
	E3,  /* exp      */
	T1,  /* tr       */
	T2,  /* tru      */
	T3,  /* true     */
	F1,  /* fa       */
	F2,  /* fal      */
	F3,  /* fals     */
	F4,  /* false    */
	N1,  /* nu       */
	N2,  /* nul      */
	N3,  /* null     */
	NR_STATES
};

static int ascii_class[128] = {
	-1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
	-1,      C_WHITE, C_WHITE, -1,      -1,      C_WHITE, -1,      -1,
	-1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
	-1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,

	C_SPACE, C_ETC,   C_QUOTE, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_PLUS,  C_COMMA, C_MINUS, C_POINT, C_SLASH,
	C_ZERO,  C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT, C_DIGIT,
	C_DIGIT, C_DIGIT, C_COLON, C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,

	C_ETC,   C_ABCDF, C_ABCDF, C_ABCDF, C_ABCDF, C_E,     C_ABCDF, C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LSQRB, C_BACKS, C_RSQRB, C_ETC,   C_ETC,

	C_ETC,   C_LOW_A, C_LOW_B, C_LOW_C, C_LOW_D, C_LOW_E, C_LOW_F, C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_ETC,   C_LOW_L, C_ETC,   C_LOW_N, C_ETC,
	C_ETC,   C_ETC,   C_LOW_R, C_LOW_S, C_LOW_T, C_LOW_U, C_ETC,   C_ETC,
	C_ETC,   C_ETC,   C_ETC,   C_LCURB, C_ETC,   C_RCURB, C_ETC,   C_ETC
};

static int state_transition_table[NR_STATES][NR_CLASSES] = {
	/*start  GO*/ {GO, GO, -6, -1, -5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*ok     OK*/ {OK, OK, -1, -8, -1, -7, -1, -3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*object OB*/ {OB, OB, -1, -9, -1, -1, -1, -1, ST, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*key    KE*/ {KE, KE, -1, -1, -1, -1, -1, -1, ST, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*colon  CO*/ {CO, CO, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*value  VA*/ {VA, VA, -6, -1, -5, -1, -1, -1, ST, -1, -1, -1, MI, -1, ZE, IN, -1, -1, -1, -1, -1, F1, -1, N1, -1, -1, T1, -1, -1, -1, -1},
	/*array  AR*/ {AR, AR, -6, -1, -5, -7, -1, -1, ST, -1, -1, -1, MI, -1, ZE, IN, -1, -1, -1, -1, -1, F1, -1, N1, -1, -1, T1, -1, -1, -1, -1},
	/*string ST*/ {ST, -1, ST, ST, ST, ST, ST, ST, -4, ES, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST, ST},
	/*escape ES*/ { -1, -1, -1, -1, -1, -1, -1, -1, ST, ST, ST, -1, -1, -1, -1, -1, -1, ST, -1, -1, -1, ST, -1, ST, ST, -1, ST, U1, -1, -1, -1},
	/*u1     U1*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, U2, U2, U2, U2, U2, U2, U2, U2, -1, -1, -1, -1, -1, -1, U2, U2, -1},
	/*u2     U2*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, U3, U3, U3, U3, U3, U3, U3, U3, -1, -1, -1, -1, -1, -1, U3, U3, -1},
	/*u3     U3*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, U4, U4, U4, U4, U4, U4, U4, U4, -1, -1, -1, -1, -1, -1, U4, U4, -1},
	/*u4     U4*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, ST, ST, ST, ST, ST, ST, ST, ST, -1, -1, -1, -1, -1, -1, ST, ST, -1},
	/*minus  MI*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, ZE, IN, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*zero   ZE*/ {OK, OK, -1, -8, -1, -7, -1, -3, -1, -1, -1, -1, -1, FR, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*int    IN*/ {OK, OK, -1, -8, -1, -7, -1, -3, -1, -1, -1, -1, -1, FR, IN, IN, -1, -1, -1, -1, E1, -1, -1, -1, -1, -1, -1, -1, -1, E1, -1},
	/*frac   FR*/ {OK, OK, -1, -8, -1, -7, -1, -3, -1, -1, -1, -1, -1, -1, FR, FR, -1, -1, -1, -1, E1, -1, -1, -1, -1, -1, -1, -1, -1, E1, -1},
	/*e      E1*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, E2, E2, -1, E3, E3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*ex     E2*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, E3, E3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*exp    E3*/ {OK, OK, -1, -8, -1, -7, -1, -3, -1, -1, -1, -1, -1, -1, E3, E3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*tr     T1*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, T2, -1, -1, -1, -1, -1, -1},
	/*tru    T2*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, T3, -1, -1, -1},
	/*true   T3*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, OK, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*fa     F1*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, F2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*fal    F2*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, F3, -1, -1, -1, -1, -1, -1, -1, -1},
	/*fals   F3*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, F4, -1, -1, -1, -1, -1},
	/*false  F4*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, OK, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	/*nu     N1*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, N2, -1, -1, -1},
	/*nul    N2*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, N3, -1, -1, -1, -1, -1, -1, -1, -1},
	/*null   N3*/ { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, OK, -1, -1, -1, -1, -1, -1, -1, -1},
};

enum modes {
	MODE_ARRAY,
	MODE_DONE,
	MODE_KEY,
	MODE_OBJECT,
};

static int reject(json_check_t *jc) {
	tree_zfree(jc);
	return FALSE;
}

static int push(json_check_t *jc, int mode) {
	jc->top += 1;
	if (jc->top >= jc->depth) {
		return FALSE;
	}
	jc->stack[jc->top] = mode;
	return TRUE;
}

static int pop(json_check_t *jc, int mode) {
	if (jc->top < 0 || jc->stack[jc->top] != mode) {
		return FALSE;
	}
	jc->top -= 1;
	return TRUE;
}

json_check_t *new_json_check(int depth) {
	json_check_t *jc = (json_check_t *)tree_zmalloc(sizeof(json_check_t), NULL);
	jc->state = GO;
	jc->depth = depth;
	jc->top = -1;
	jc->stack = tree_zcalloc(depth, sizeof(int), jc);
	push(jc, MODE_DONE);
	return jc;
}

int json_check_char(json_check_t *jc, int next_char) {
	int next_class, next_state;
	if (next_char < 0) {
		return reject(jc);
	}
	if (next_char >= 128) {
		next_class = C_ETC;
	} else {
		next_class = ascii_class[next_char];
		if (next_class <= -1) {
			return reject(jc);
		}
	}
	next_state = state_transition_table[jc->state][next_class];
	if (next_state >= 0) {
		jc->state = next_state;
	} else {
		switch (next_state) {
			case -9:
				if (!pop(jc, MODE_KEY)) {
					return reject(jc);
				}
				jc->state = OK;
				break;

			case -8:
				if (!pop(jc, MODE_OBJECT)) {
					return reject(jc);
				}
				jc->state = OK;
				break;

			case -7:
				if (!pop(jc, MODE_ARRAY)) {
					return reject(jc);
				}
				jc->state = OK;
				break;

			case -6:
				if (!push(jc, MODE_KEY)) {
					return reject(jc);
				}
				jc->state = OB;
				break;

			case -5:
				if (!push(jc, MODE_ARRAY)) {
					return reject(jc);
				}
				jc->state = AR;
				break;

			case -4:
				switch (jc->stack[jc->top]) {
					case MODE_KEY:
						jc->state = CO;
						break;
					case MODE_ARRAY:
					case MODE_OBJECT:
						jc->state = OK;
						break;
					default:
						return reject(jc);
				}
				break;

			case -3:
				switch (jc->stack[jc->top]) {
					case MODE_OBJECT:
						if (!pop(jc, MODE_OBJECT) || !push(jc, MODE_KEY)) {
							return reject(jc);
						}
						jc->state = KE;
						break;
					case MODE_ARRAY:
						jc->state = VA;
						break;
					default:
						return reject(jc);
				}
				break;

			case -2:
				if (!pop(jc, MODE_KEY) || !push(jc, MODE_OBJECT)) {
					return reject(jc);
				}
				jc->state = VA;
				break;
			default:
				return reject(jc);
		}
	}
	return TRUE;
}

int json_check_done(json_check_t *jc) {
	int result = jc->state == OK && pop(jc, MODE_DONE);
	reject(jc);
	return result;
}

bool json_valid(const char *json) {
	json_check_t *jc = new_json_check(JSON_CHECK_DEPTH);
	while (*json) {
		int cnext = *json++;
		if (cnext <= 0) {
			break;
		}
		if (!json_check_char(jc, cnext)) {
			return FALSE;
		}
	}
	if (!json_check_done(jc)) {
		return FALSE;
	}
	return TRUE;
}
