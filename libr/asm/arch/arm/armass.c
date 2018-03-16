/* radare - LGPL - Copyright 2010-2017 - pancake */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <r_util.h>

// TODO: only lo registers accessible in thumb arm

typedef struct {
	ut64 off;
	ut32 o;
	char op[128];
	char opstr[128];
	char *a[16]; /* only 15 arguments can be used! */
} ArmOpcode;

typedef struct {
	const char *name;
	int code;
	int type;
} ArmOp;

enum {
	TYPE_MOV = 1,
	TYPE_TST = 2,
	TYPE_SWI = 3,
	TYPE_HLT = 4,
	TYPE_BRA = 5,
	TYPE_BRR = 6,
	TYPE_ARI = 7,
	TYPE_IMM = 8,
	TYPE_MEM = 9,
	TYPE_BKP = 10,
	TYPE_SWP = 11,
	TYPE_MOVW = 12,
	TYPE_MOVT = 13,
	TYPE_UDF = 14,
	TYPE_SHFT = 15,
	TYPE_COPROC = 16,
	TYPE_ENDIAN = 17,
	TYPE_MUL = 18,
	TYPE_CLZ = 19,
	TYPE_REV = 20,
};

static int strcmpnull(const char *a, const char *b) {
	if (!a || !b)
		return -1;
	return strcmp (a, b);
}

// static const char *const arm_shift[] = {"lsl", "lsr", "asr", "ror"};

static ArmOp ops[] = {
	{ "adc", 0xa000, TYPE_ARI },
	{ "adcs", 0xb000, TYPE_ARI },
	{ "adds", 0x9000, TYPE_ARI },
	{ "add", 0x8000, TYPE_ARI },
	{ "bkpt", 0x2001, TYPE_BKP },
	{ "subs", 0x5000, TYPE_ARI },
	{ "sub", 0x4000, TYPE_ARI },
	{ "sbcs", 0xd000, TYPE_ARI },
	{ "sbc", 0xc000, TYPE_ARI },
	{ "rsb", 0x6000, TYPE_ARI },
	{ "rsbs", 0x7000, TYPE_ARI },
	{ "rsc", 0xe000, TYPE_ARI },
	{ "rscs", 0xf000, TYPE_ARI },
	{ "bic", 0x0000c0e1, TYPE_ARI },

	{ "udf", 0xf000f000, TYPE_UDF },

	{ "push", 0x2d09, TYPE_IMM },
	{ "pop", 0xbd08, TYPE_IMM },

	{ "cps", 0xb1, TYPE_IMM },
	{ "nop", 0xa0e1, -1 },

	{ "ldrex", 0x9f0f9000, TYPE_MEM },
	{ "ldr", 0x9000, TYPE_MEM },

	{ "strexh", 0x900fe000, TYPE_MEM },
	{ "strexb", 0x900fc000, TYPE_MEM },
	{ "strex", 0x900f8000, TYPE_MEM },
	{ "strbt", 0x0000e0e4, TYPE_MEM },
	{ "strb", 0x0000c0e5, TYPE_MEM },
	{ "strd", 0xf000c0e1, TYPE_MEM },
	{ "strh", 0xb00080e1, TYPE_MEM },
	{ "str", 0x8000, TYPE_MEM },

	{ "blx", 0x30ff2fe1, TYPE_BRR },
	{ "bx", 0x10ff2fe1, TYPE_BRR },

	{ "bl", 0xb, TYPE_BRA },
// bx/blx - to register, b, bne,.. justjust  offset
//    2220:       e12fff1e        bx      lr
//    2224:       e12fff12        bx      r2
//    2228:       e12fff13        bx      r3

	//{ "bx", 0xb, TYPE_BRA },
	{ "b", 0xa, TYPE_BRA },

	//{ "mov", 0x3, TYPE_MOV },
	//{ "mov", 0x0a3, TYPE_MOV },
	{ "movw", 0x3, TYPE_MOVW },
	{ "movt", 0x4003, TYPE_MOVT },
	{ "mov", 0xa001, TYPE_MOV },
	{ "mvn", 0xe000, TYPE_MOV },
	{ "svc", 0xf, TYPE_SWI }, // ???
	{ "hlt", 0x70000001, TYPE_HLT }, // ???u

	{ "mul", 0x900000e0, TYPE_MUL},
	{ "smull", 0x9000c0e0, TYPE_MUL},
	{ "umull", 0x900080e0, TYPE_MUL},
	{ "smlal", 0x9000e0e0, TYPE_MUL},
	{ "smlabb", 0x800000e1, TYPE_MUL},
	{ "smlabt", 0xc00000e1, TYPE_MUL},
	{ "smlatb", 0xa00000e1, TYPE_MUL},
	{ "smlatt", 0xe00000e1, TYPE_MUL},
	{ "smlawb", 0x800020e1, TYPE_MUL},
	{ "smlawt", 0xc00020e1, TYPE_MUL},


	{ "ands", 0x1000, TYPE_ARI },
	{ "and", 0x0000, TYPE_ARI },
	{ "eors", 0x3000, TYPE_ARI },
	{ "eor", 0x2000, TYPE_ARI },
	{ "orrs", 0x9001, TYPE_ARI },
	{ "orr", 0x8001, TYPE_ARI },

	{ "cmp", 0x5001, TYPE_TST },
	{ "swp", 0xe1, TYPE_SWP },
	{ "cmn", 0x0, TYPE_TST },
	{ "teq", 0x0, TYPE_TST },
	{ "tst", 0xe1, TYPE_TST },

	{"lsr", 0x3000a0e1, TYPE_SHFT},
	{"asr", 0x5000a0e1, TYPE_SHFT},
	{"lsl", 0x1000a0e1, TYPE_SHFT},
	{"ror", 0x7000a0e1, TYPE_SHFT},

	{"rev16", 0xb00fbf06, TYPE_REV},
	{"revsh", 0xb00fff06, TYPE_REV},
	{"rev",   0x300fbf06, TYPE_REV},
	{"rbit",  0x300fff06, TYPE_REV},

	{"mrc", 0x100010ee, TYPE_COPROC},
	{"setend", 0x000001f1, TYPE_ENDIAN},
	{ "clz", 0x000f6f01, TYPE_CLZ},

	{ NULL }
};

static ut32 M_BIT = 0x01;
static ut32 S_BIT = 0x02;
// UNUSED static ut32 C_BITS = 0x3c;
static ut32 DOTN_BIT = 0x40;
static ut32 DOTW_BIT = 0x80;

ut32 opmask(char *input, char *opcode) {
	ut32 res = 0;
	ut32 i;
	
	const char *conds[] = {
		"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
		"hi", "ls", "ge", "lt", "gt", "le", "al", "nv", 0
	};

	// const char *qs[] = {NULL, ".n", ".w", 0};

	r_str_case (input, false);

	if (strlen (opcode) > strlen (input)) {
		return 0;
	}

	if (!strncmp (input, opcode, strlen (opcode))) {
		input += strlen (opcode);
		res |= M_BIT;
		if (input[0] == 0) {
			return res;
		}

		if (input[0] == 's') {
			res |= S_BIT;
			input++;
		}
		if (input[0]==0) {
			return res;
		}
		for (i = 0; conds[i]; i++) {
			if (!strncmp (input, conds[i], strlen (conds[i]))) {
				res |= i << 2;
				input+= strlen (conds[i]);
				break;
			}
		}
		if (!conds[i]) {
			// default is nv (no value)
			res |= 15 << 2;
		}
		if (input[0]==0) {
			return res;
		}

		if (!strncmp (input, ".n", 2)) {
			res |= 1 << 6;
		}

		if (!strncmp (input, ".w", 2)) {
			res |= 1 << 7;
		}
	}
	return res;
}

static bool err;
//decode str as number
static int getnum(const char *str) {
	char *endptr;
	err = false;
	double val;

	if (!str) {
		err = true;
		return 0;
	}
	while (*str == '$' || *str == '#') {
		str++;
	}
	val = strtod (str, &endptr);
	if (str != endptr && *endptr == '\0') {
		return val;
	}
	err = true;
	return 0;
}

static ut32 getimmed8(const char *str) {
	ut32 num = getnum (str);
	if (err) {
		return 0;
	}
	ut32 rotate;
	if (num <= 0xff) {
		return num;
	} else {
		for (rotate = 1; rotate < 16; rotate++) {
			// rol 2
			num = ((num << 2) | (num >> 30));
			if (num == (num & 0xff)) {
				return (num | (rotate << 8));
			}
		}
		err = 1;
		return 0;
	}
}

st32 firstsigdigit (ut32 num) {
	st32 f = -1;
	st32 b = -1;
	ut32 forwardmask = 0x80000000;
	ut32 backwardmask = 0x1;
	ut32 i;
	for (i = 0; i < 32; i++ ) {
		if ( (forwardmask & num) && (f == -1)) {
			f = i;
		}
		if ( (backwardmask & num) && (b == -1)) {
			b = 32-i;
		}
		forwardmask >>= 1;
		backwardmask <<= 1;
	}

	if ((b-f) < 9) {
		return f;
	}
	return -1;
}

static ut32 getthimmed12(const char *str) {
	ut32 num = getnum (str);
	if (err) {
		return 0;
	}
	st32 FSD = 0;
	ut32 result = 0;
	if (num <= 0xff) {
		return num << 8;
	} else 	if ( ((num & 0xff00ff00) == 0) && ((num & 0x00ff0000) == ((num & 0x000000ff) << 16)) ) {
		result |= (num & 0x000000ff) << 8;
		result |= 0x00000010;
		return result;
	} else if ( ((num & 0x00ff00ff) == 0) && ((num & 0xff000000) == ((num & 0x0000ff00) << 16)) ) {
		result |= num & 0x0000ff00;
		result |= 0x00000020;
		return result;
	} else if ( ((num & 0xff000000) == ((num & 0x00ff0000) << 8)) && ((num & 0xff000000) == ((num & 0x0000ff00) << 16)) && ((num &0xff000000) == ((num & 0x000000ff) << 24)) ) {
		result |= num & 0x0000ff00;
		result |= 0x00000030;
		return result;
	} else {
		FSD = firstsigdigit(num);
		if (FSD != -1) {
		        result |= ((num >> (24-FSD)) & 0x0000007f) << 8;
			result |= ((8+FSD) & 0x1) << 15;
			result |= ((8+FSD) & 0xe) << 3;
			result |= ((8+FSD) & 0x10) << 14;
			return result;
		} else {
			err = true;
			return 0;
		}
	}
}

static char *getrange(char *s) {
	char *p = NULL;
	while (s && *s) {
		if (*s == ',') {
			p = s+1;
			*p=0;
		}
		if (*s == '[' || *s == ']')
			memmove (s, s+1, strlen (s+1)+1);
		if (*s == '}')
			*s = 0;
		s++;
	}
	while (p && *p == ' ') p++;
	return p;
}

#if 0
static int getshift_unused (const char *s) {
	int i;
	const char *shifts[] = { "lsl", "lsr", "asr", "ror", NULL };
	for (i=0; shifts[i]; i++)
		if (!strcmpnull (s, shifts[i]))
			return i * 0x20;
	return 0;
}
#endif

//ret register #; -1 if failed
static int getreg(const char *str) {
	int i;
	char *ep;
	const char *aliases[] = { "sl", "fp", "ip", "sp", "lr", "pc", NULL };
	if (!str || !*str) {
		return -1;
	}
	if (*str == 'r') {
		int reg = strtol (str + 1, &ep, 10);
		if ((ep[0] != '\0') || (str[1] == '\0')) {
			return -1;
		}
		if (reg < 16 && reg >= 0) {
			return reg;
		}
	}
	for (i=0; aliases[i]; i++) {
		if (!strcmpnull (str, aliases[i])) {
			return 10 + i;
		}
	}
	return -1;
}

static int thumb_getreg(const char *str) {
	if (!str)
		return -1;
	if (*str == 'r')
		return atoi (str+1);
	//FIXME Note that pc is only allowed in pop; lr in push in Thumb1 mode.
	if (!strcmpnull (str, "pc") || !strcmpnull(str,"lr"))
		return 8;
	return -1;
}

static int getlist(char *op) {
	int reg, list = 0;
	char *ptr = strchr (op, '{');
	if (ptr) {
		do {
			ptr++;
			while (*ptr && *ptr == ' ') ptr++;
			reg = getreg (ptr);
			if (reg == -1)
				break;
			list |= (1 << reg);
			while (*ptr && *ptr!=',') ptr++;
		} while (*ptr && *ptr == ',');
	}
	return list;
}

static ut32 thumb_getshift(const char *str) {
	// only immediate shifts are ever used by thumb-2. Bit positions are different from ARM.
	const char *shifts[] = {
		"LSL", "LSR", "ASR", "ROR", 0, "RRX"
	};
	char *type = strdup (str);
	char *arg;
	char *space;
	ut32 res = 0;
	ut32 shift = false;
	err = false;
	ut32 argn;
	ut32 i;
	
	r_str_case (type,true);
	
	if (!strcmp (type, shifts[5])) {
		// handle RRX alias case
		res |= 3 << 12;	
		free (type);
		return res;
	}
	
	space = strchr (type, ' ');
	if (!space) {
		free (type);
		err = true;
		return 0;
	}
	*space = 0;
	arg = strdup (++space);
	
	for (i = 0; shifts[i]; i++) {
		if (!strcmp (type, shifts[i])) {
			shift = true;
			break;
		}
	}
	if (!shift) {
		err = true;
		free (type);
		free (arg);
		return 0;
	}
	res |= i << 12;
		
	argn = getnum (arg);
	if (err || argn > 32) {
		err = true;
		free (type);
		free (arg);
		return 0;
	}
	res |= ( (argn & 0x1c) << 2);
	res |= ( (argn & 0x3) << 14);

	free (type);
	free (arg);
	return res;
}

static ut64 thumb_selector(char *args[]) {
	ut64 res = 0;
	ut8 i;
	
	for (i = 0; i < 15; i++) {
		if (args[i] == NULL) {
			break;
		}
		if (getreg (args[i]) != -1) {
			res |= 1 << (i*4);
			continue;
		}
		
		err = false;
		getnum (args[i]);
		if (!err) {
			res |= 2 << (i*4);
			continue;
		}

		err = false;		
		thumb_getshift (args[i]);
		if (!err) {
			res |= 3 << (i*4);
			continue;
		}
		res |= 0xf << (i*4);
	}
	err = false;
	return res;
}
		
static ut32 getshift(const char *str) {
	char type[128];
	char arg[128];
	char *space;
	ut32 i=0, shift=0;
	const char *shifts[] = {
		"LSL", "LSR", "ASR", "ROR",
		0, "RRX" // alias for ROR #0
	};

	strncpy (type, str, sizeof (type) - 1);
	// XXX strcaecmp is probably unportable
	if (!strcasecmp (type, shifts[5])) {
		// handle RRX alias case
		shift = 6;
	} else { // all other shift types
		space = strchr (type, ' ');
		if (!space) {
			return 0;
		}
		*space = 0;
		strncpy (arg, ++space, sizeof(arg) - 1);

		for (i = 0; shifts[i]; i++) {
			if (!strcasecmp (type, shifts[i])) {
				shift = 1;
				break;
			}
		}
		if (!shift) {
			return 0;
		}
		shift = i * 2;
		if ((i = getreg (arg)) != -1) {
			i <<= 8; // set reg
//			i|=1; // use reg
			i |= (1 << 4); // bitshift
			i |= shift << 4; // set shift mode
			if (shift == 6) i |= (1 << 20);
		} else {
			char *bracket = strchr (arg, ']');
			if (bracket) {
				*bracket = '\0';
			}
			// ensure only the bottom 5 bits are used
			i &= 0x1f;
			if (!i) i = 32;
			i = (i * 8);
			i |= shift; // lsl, ror, ...
			i = i << 4;
		}
	}

	return i;
}

static void arm_opcode_parse(ArmOpcode *ao, const char *str) {
	int i;
	memset (ao, 0, sizeof (ArmOpcode));
	if (strlen (str)+1>=sizeof (ao->op))
		return;
	strncpy (ao->op, str, sizeof (ao->op)-1);
	strcpy (ao->opstr, ao->op);
	ao->a[0] = strchr (ao->op, ' ');
	for (i=0; i<15; i++) {
		if (ao->a[i]) {
			*ao->a[i] = 0;
			ao->a[i+1] = strchr (++ao->a[i], ',');
		} else break;
	}
	if (ao->a[i]) {
		*ao->a[i] = 0;
		ao->a[i]++;
	}
	for (i=0; i<16; i++) {
		while (ao->a[i] && *ao->a[i] == ' ') {
			ao->a[i]++;
		}
	}
}

static inline int arm_opcode_cond(ArmOpcode *ao, int delta) {
	const char *conds[] = {
		"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
		"hi", "ls", "ge", "lt", "gt", "le", "al", "nv", 0
	};
	int i, cond = 14; // 'always' is default
	char *c = ao->op+delta;
	for (i=0; conds[i]; i++) {
		if (!strcmpnull (c, conds[i])) {
			cond = i;
			break;
		}
	}
	ao->o |= cond << 4;
	return cond;
}

static void thumb_swap (ut32 *a) {
	ut32 a2 = *a;
	ut8 *b = (ut8 *)a;
	ut8 *b2 = (ut8 *) & a2;
	b[0] = b2[1];
	b[1] = b2[0];
	b[2] = b2[3];
	b[3] = b2[2];
}

// TODO: group similar instructions like for non-thumb
static int thumb_assemble(ArmOpcode *ao, ut64 off, const char *str) {
	int reg, j;
	ut32 m;
	ao->o = UT32_MAX;
	if (!strcmpnull (ao->op, "pop") && ao->a[0]) {
		ao->o = 0xbc;
		if (*ao->a[0] ++== '{') {
			// XXX: inverse order?
			for (j=0; j<16; j++) {
				if (ao->a[j] && *ao->a[j]) {
					int sr, er;
					char *ers;
					getrange (ao->a[j]); // XXX filter regname string
					sr = er = thumb_getreg (ao->a[j]);
					if ((ers = strchr (ao->a[j], '-'))) { // register sequence
						er = thumb_getreg (ers+1);
					}
					for (reg = sr; reg <= er; reg++) {
						if (reg != -1) {
							if (reg < 8)
								ao->o |= 1 << (8 + reg);
							if (reg == 8) {
								ao->o |= 1;
							}
							//	else ignore...
						}
					}
				}
			}
		} else ao->o |= getnum (ao->a[0]) << 24; // ???
		return 2;
	} else
	if (!strcmpnull (ao->op, "push") && ao->a[0]) {
		ao->o = 0xb4;
		if (*ao->a[0] ++== '{') {
			for (j = 0; j < 16; j++) {
				if (ao->a[j] && *ao->a[j]) {
					getrange (ao->a[j]); // XXX filter regname string
					reg = thumb_getreg (ao->a[j]);
					if (reg != -1) {
						if (reg < 8)
							ao->o |= 1 << (8 + reg);
						if (reg == 8)
							ao->o |= 1;
					//	else ignore...
					}
				}
			}
		} else ao->o |= getnum (ao->a[0]) << 24; // ???
		return 2;
	} else
	if (!strcmpnull (ao->op, "ldmia")) {
		ao->o = 0xc8 + getreg (ao->a[0]);
		ao->o |= getlist(ao->opstr) << 8;
		return 2;
	} else
	if (!strcmpnull (ao->op, "stmia")) {
		ao->o = 0xc0 + getreg (ao->a[0]);
		ao->o |= getlist(ao->opstr) << 8;
		return 2;
	} else
	if (!strcmpnull (ao->op, "cbz")) {
		ut64 dst = getnum (ao->a[1]);
		int delta = dst - off;
		if (delta < 4) {
			return -1;
		}
		ao->o = 0xb1;
		ao->o += getreg (ao->a[0]) << 8;
		delta -= 4;
		delta /= 4;
		ao->o |= delta << 12;
		return 2;
	} else
	if (!strcmpnull (ao->op, "nop")) {
		ao->o = 0xbf;
		return 2;
	} else
	if (!strcmpnull (ao->op, "yield")) {
		ao->o = 0x10bf;
		return 2;
	} else
	if (!strcmpnull (ao->op, "udf")) {
		ao->o = 0xde;
		ao->o |= getnum (ao->a[0]) << 8;
		return 2;
	} else
	if (!strcmpnull (ao->op, "wfe")) {
		ao->o = 0x20bf;
		return 2;
	} else
	if (!strcmpnull (ao->op, "wfi")) {
		ao->o = 0x30bf;
		return 2;
	} else
	if (!strcmpnull (ao->op, "sev")) {
		ao->o = 0x40bf;
		return 2;
	} else
	if (!strcmpnull (ao->op, "bkpt")) {
		ao->o = 0xbe;
		ao->o |= (0xff & getnum (ao->a[0])) << 8;
		return 2;
	} else
#if 0
	if (!strcmpnull (ao->op, "and")) {
		ao->o = 0x40;
		ao->o |= (0xff & getreg (ao->a[0])) << 8;
		ao->o |= (0xff & getreg (ao->a[1])) << 11;
	} else
#endif
	if (!strcmpnull (ao->op, "svc")) {
		ao->o = 0xdf;
		ao->o |= (0xff & getnum (ao->a[0])) << 8;
		return 2;
	} else
	if (!strcmpnull (ao->op, "b") || !strcmpnull (ao->op, "b.n")) {
		//uncond branch : PC += 4 + (delta*2)
		int offset = getnum (ao->a[0]);
		if (err) {
			return 0;
		}
		int delta = offset - 4 - ao->off;
		if ((delta < -2048) || (delta > 2046) || (delta & 1)) {
			eprintf("branch out of range or not even\n");
			return 0;
		}
		ut16 opcode = 0xe000 | ((delta / 2) & 0x7ff);	//11bit offset>>1
		ao->o = opcode >> 8;
		ao->o |= (opcode & 0xff) << 8;    // XXX (ut32) ao->o holds the opcode in little-endian format !?
		return 2;
	} else
	if (!strcmpnull (ao->op, "bx")) {
		ao->o = 0x47;
		ao->o |= getreg (ao->a[0]) << 11;
		return 2;
	} else
	if (!strcmpnull (ao->op, "blx") || !strcmpnull (ao->op, "bl")) {
		int reg = getreg (ao->a[0]);
		if (reg == -1) {
			ut64 n = r_num_math (NULL, ao->a[0]);
			ut64 pc = ao->off + 4;
			if (ao->op[2] == 'x') {
				n |= pc & 0x2; // care for alignment
				ao->o = 0xf000e800;
			} else {
				ao->o = 0xf000f800;
			}
			n -= pc;
			if ((n < (-1 * (1 << 22)) &&
			    (n > ((1 << 22) - 2)))) {
				eprintf("branch out of range or not even\n");
				return 0;
			}
			int l = (n >> 1) & 0x7ff;
			int h = (n >> 12) & 0x7ff;
			ao->o |= h << 16 | l;
			thumb_swap (&ao->o);
		} else {
			if (ao->op[2] == 'x') {
				ao->o = 0x8047;
				ao->o |= reg << 11;
			} else {
				eprintf("bad parameter\n");
				return 0;
			}
		}
		// XXX: length = 4
		return 4;
	} else
	if (*ao->op == 'b') { // conditional branch
		ao->o = 0xd0 | arm_opcode_cond (ao, 1);
		int bdelta = getnum (ao->a[0]) -4;
		if ((bdelta < -256) || (bdelta > 254) || (bdelta & 1)) {
			eprintf("branch out of range or not even\n");
			return 0;
		}
		ao->o |= ((bdelta/2) & 0xff) << 8;	// 8bit offset >>1
		return 2;
	} else
	if (!strcmpnull (ao->op, "mov")) {
		int reg = getreg (ao->a[1]);
		if (reg != -1) {
			ao->o = 0x46;
			ao->o |= (getreg (ao->a[0]) & 0x8) << 12;
			ao->o |= (getreg (ao->a[0]) & 0x7) << 8;
			ao->o |= reg << 11;
		} else {
			ao->o = 0x20;
			ao->o |= (getreg (ao->a[0]));
			ao->o |= (getnum (ao->a[1]) & 0xff) << 8;
		}
		return 2;
	} else
	if (!strcmpnull (ao->op, "mov.w")) {
		ao->o = 0x4ff00000;
		int reg = getreg (ao->a[0]);
		int num = getnum (ao->a[1]);
		int top_bits = num & 0xf00;
		if (reg != -1) {
			ao->o |= reg;
			if (num < 256) {
				ao->o |= num << 8;
			} else if (num < 512) {
				if (num & 1) {
					return 0;
				}
				num = (num ^ top_bits) >> 1;
				ao->o |= 0x00048070 | num << 8;
			} else if (num < 1024) {
				if (num & 3) {
					return 0;
				}
				num = (num ^ top_bits) >> 2;
				ao->o |= 0x00040070 | num << 8;
			} else if (num < 2048) {
				if (num & 7) {
					return 0;
				}
				num = (num ^ top_bits) >> 3;
				ao->o |= 0x00048060 | num << 8;
			} else if (num == 2048) {
				ao->o = 0x4ff40060 | reg;
			} else {
				return 0;
			}
		} else {
			return 0;
		}
		return 8;
	} else
	if (!strncmp (ao->op, "ldr", 3)) {
		char ch = (ao->op[3] == '.') ? ao->op[4] : ao->op[3];
		getrange (ao->a[1]);
		getrange (ao->a[2]);
		switch (ch) {
		case 'w': {
			int a0 = getreg (ao->a[0]);
			int a1 = getreg (ao->a[1]);
			int a2 = getnum (ao->a[2]);
			ao->o = 0xd0f80000;
			ao->o |= ((a2 & 0xff) << 8);
			ao->o |= (((a2 >> 8) & 0xff));
			ao->o |= (a0 << 4);
			ao->o |= (a1 << 24);
			return 4;
			}
			break;
		case 'h': {
			int a0 = getreg (ao->a[0]);
			int a1 = getreg (ao->a[1]);
			int a2 = getreg (ao->a[2]);
			if (a2 ==-1) {
				a2 = getnum (ao->a[2]) / 8;
				ao->o = 0x88; // | (8+(0xf & a0));
				ao->o |= (7 & a0) << 8;
				ao->o |= (7 & a1) << 11;
				ao->o += (7 & a2);
				return 2;
			} else return 0; }
		case 'b': {
			int a0 = getreg (ao->a[0]);
			int a1 = getreg (ao->a[1]);
			int a2 = getreg (ao->a[2]);
			if (a2 ==-1) {
				a2 = getnum (ao->a[2]) / 8;
				ao->o = 0x78; // | (8+(0xf & a0));
				ao->o |= (7 & a0) << 8;
				ao->o |= (7 & a1) << 11;
				ao->o |= (7 & a2);
				return 2;
			} else return 0; }
		default: {
			if (!strcmpnull (ao->a[1], "sp")) {
				// ldr r0, [sp, n] = a[r0-7][nn]
				if (getreg (ao->a[2]) == -1) {
					// ldr r0, [sp, n]
					ao->o = 0x98 + (0xf & getreg (ao->a[0]));
					ao->o |= (0xff & getnum (ao->a[2]) / 4) << 8;
					return 2;
				} else return 0;
			} else
			if (!strcmpnull (ao->a[1], "pc")) {
				// ldr r0, [pc, n] = 4[r0-8][nn*4]
				if (getreg (ao->a[2]) == -1) {
					ao->o = 0x40 | (8 + (0xf & getreg (ao->a[0])));
					ao->o |= (0xff & getnum (ao->a[2]) / 4) << 8;
					return 2;
				} else return 0;
			} else {
				// ldr r0, [rN, rN] = 58[7bits:basereg + 7bits:destreg]
				int a0 = getreg (ao->a[0]);
				int a1 = getreg (ao->a[1]);
				int a2 = getreg (ao->a[2]);
				if (a2 == -1) {
					int num= getnum (ao->a[2]);
					if (num > 0) {
						if (num >= 0x1000) {
							return -1;
						}
						if (num < 0x78 &&
						    (!(num & 0xb) || !(num & 0x7)) &&
						    (a0 < 8 && a1 < 8)) {
							ao->o = 0x68;
							ao->o |= (num & 0xf) << 12;
							ao->o |= a1 << 11;
							ao->o |= a0 << 8;
							ao->o |= num >> 4;
							return 2;
						}
						// ldr r0, [rN, num]
						// Separate out high bits
						int h = num >> 8;
						num &= 0xff;
						ao->o = (0xd0 | a1) << 24;
						ao->o |= 0xf8 << 16;
						ao->o |= num << 8;
						ao->o |= a0 << 4;
						ao->o |= h;
					} else {
						// ldr r0, [rN]
						ao->o = 0x68;
						ao->o |= (7 & a0) << 8;
						ao->o |= (7 & a1) << 11;
					}
				} else {
					ao->o = 0x58; // | (8+(0xf & a0));
					ao->o |= (7 & a0) << 8;
					ao->o |= (7 & a1) << 11;
					ao->o |= (7 & a2) << 14;
				}
				return 2;
			}
		} }
	} else
	if (!strncmp (ao->op, "str", 3)) {
		getrange (ao->a[1]);
		getrange (ao->a[2]);
		if (ao->op[3] == 'h') {
			int a0 = getreg (ao->a[0]);
			int a1 = getreg (ao->a[1]);
			int a2 = getreg (ao->a[2]);
			if (a2 == -1) {
				a2 = getnum (ao->a[2]);
				ao->o = 0x80; // | (8+(0xf & a0));
				ao->o |= (7 & a0) << 8;
				ao->o |= (7 & a1) << 11;
				ao->o |= (7 & (a2 >> 1));
				return 2;
			}
		} else
		if (ao->op[3] == 'b') {
			int a0 = getreg (ao->a[0]);
			int a1 = getreg (ao->a[1]);
			int a2 = getreg (ao->a[2]);
			if (a2 == -1) {
				a2 = getnum (ao->a[2]);
				ao->o = 0x70; // | (8+(0xf & a0));
				ao->o |= (7 & a0) << 8;
				ao->o |= (7 & a1) << 11;
				ao->o |= (7 & a2);
				return 2;
			}
		} else {
			if (!strcmpnull (ao->a[1], "sp")) {
				// ldr r0, [sp, n] = a[r0-7][nn]
				if (getreg (ao->a[2]) == -1) {
					int ret = getnum (ao->a[2]);
					if (ret%4) {
						eprintf ("ldr index must be aligned to 4");
						return 0;
					}
					ao->o = 0x90 + (0xf & getreg (ao->a[0]));
					ao->o |= (0xff & getnum (ao->a[2]) / 4) << 8;
					return 2;
				}
			} else
			if (!strcmpnull (ao->a[1], "pc")) {
				return 0;
			} else {
				int a0 = getreg (ao->a[0]);
				int a1 = getreg (ao->a[1]);
				int a2 = getreg (ao->a[2]);
				if (a2 == -1) {
					a2 = getnum (ao->a[2]);
					ao->o = 0x60;
					ao->o |= (7 & a0) << 8;
					ao->o |= (7 & a1) << 11;
					ao->o |= (3 & (a2 / 4)) << 14;
					ao->o |= ((28 & (a2 / 4)) / 4);
				} else {
					ao->o = 0x50;
					ao->o |= (7 & a0) << 8;
					ao->o |= (7 & a1) << 11;
					ao->o |= (3 & a2) << 14;
				}
				return 2;
			}
		}
	} else
	if (!strcmpnull (ao->op, "tst")) {
		ao->o = 0x42;
		ao->o |= (getreg (ao->a[0])) << 8;
		ao->o |= getreg (ao->a[1]) << 11;
		return 2;
	} else
	if (!strcmpnull (ao->op, "cmp")) {
		int reg = getreg (ao->a[1]);
		if (reg != -1) {
			ao->o = 0x45;
			ao->o |= (getreg (ao->a[0])) << 8;
			ao->o |= reg << 11;
		} else {
			ao->o = 0x20;
			ao->o |= 8 + (getreg (ao->a[0]));
			ao->o |= (getnum (ao->a[1]) & 0xff) << 8;
		}
		return 2;
	} else
	if (!strcmpnull (ao->op, "and") || !strcmpnull (ao->op, "and.w")) {
		int reg0 = getreg (ao->a[0]);
		int reg1 = getreg (ao->a[1]);
		int reg2 = getreg (ao->a[2]);
		if (reg0!=-1 && reg1 != -1) {
			if (reg2 == -1) {
				reg0 = getreg (ao->a[0]);
				reg1 = getreg (ao->a[0]);
				reg2 = getreg (ao->a[1]);
			}
			ao->o = 0;
			ao->o |= 0x00 | reg1;
			ao->o <<= 8;
			ao->o |= 0xea;
			ao->o <<= 8;
			ao->o |= 0x00 | reg2;
			ao->o <<= 8;
			ao->o |= 0xf0 | reg0;
			return 4;
		}
	} else
	if (!strcmpnull (ao->op, "mul") || !strcmpnull (ao->op, "mul.w")) {
		int reg0 = getreg (ao->a[0]);
		int reg1 = getreg (ao->a[1]);
		int reg2 = getreg (ao->a[2]);
		if (reg0!=-1 && reg1 != -1) {
			if (reg2 == -1) {
				reg0 = getreg (ao->a[0]);
				reg1 = getreg (ao->a[0]);
				reg2 = getreg (ao->a[1]);
			}
			ao->o = 0;
			ao->o |= 0x00 | reg1;
			ao->o <<= 8;
			ao->o |= 0xfb;
			ao->o <<= 8;
			ao->o |= 0x00 | reg2;
			ao->o <<= 8;
			ao->o |= 0xf0 | reg0;
			return 4;
		}
	} else
	if (!strcmpnull (ao->op, "add")) {
		// XXX: signed unsigned ??
		// add r, r = 44[7bits,7bits]
		// adds r, n = 3[r0-7][nn]
		int reg3 = getreg (ao->a[2]);
		if (reg3 != -1) {
			return -1;
		}
		int reg = getreg (ao->a[1]);
		if (reg != -1) {
			ao->o = 0x44;
			ao->o |= (getreg (ao->a[0])) << 8;
			ao->o |= reg << 11;
		} else {
			if (reg < 10) {
				int num = getnum (ao->a[1]);
				if (err) {
					return 0;
				}
				if (getreg (ao->a[0]) == 13 &&
				    (!(num & 0xb) || !(num & 0x7))) {
					ao->o = 0x04b0;
					ao->o |= num << 6;
					return 2;
				}
				if (num > 0xff) {
					if (num % 2) {
						return 0;
					}
					ao->o = 0x00f58070;
					ao->o |= (num >> 8) << 16;
					ao->o |= (num & 0xff) << 7;
				} else {
					ao->o = 0x00f10000;
					ao->o |= num << 8;
				}
				ao->o |= getreg (ao->a[0]) << 24;
				ao->o |= getreg (ao->a[0]);
			}
			/*ao->o = 0x30;
			ao->o |= (getreg (ao->a[0]));
			ao->o |= (getnum (ao->a[1]) & 0xff) << 8;*/
		}
		return 2;
	} else
	if ((m = opmask (ao->op, "adc"))) {
		ut64 argt = thumb_selector(ao->a);
		switch (argt) {
		case 0x21: {
			ao->a[2] = ao->a[1];
			ao->a[1] = ao->a[0];
		        }
			// intentional fallthrough
			// a bit naughty, perhaps?
		case 0x211: {
			if (m & DOTN_BIT) {
				// this is explicitly an error
				return -1;
			}
			ao->o = 0x40f10000;
			ao->o |= (getreg (ao->a[0]));
			ao->o |= getreg (ao->a[1]) << 24;
			ao->o |= getthimmed12(ao->a[2]);
			if (m & S_BIT) {
				ao->o |= 1 << 28;
			}
			return 4;
		        }
			break;
		case 0x11: {
			ut8 reg1 = getreg (ao->a[0]);
			ut8 reg2 = getreg (ao->a[1]);
			if ( (reg1 < 8) && (reg2 < 8) && !(m & DOTW_BIT)) {
				ao->o = 0x4041;
				ao->o |= (reg1 << 8);
				ao->o |= (reg2 << 11);
				return 2;
			}
			ao->a[2] = ao->a[1];
			ao->a[1] = ao->a[0];
		        }
			// intentional fallthrough
			// a bit naughty, perhaps?
		case 0x111: {
			if (m & DOTN_BIT) {
				// this is explicitly an error
				return -1;
			}
			ao->o = 0x40eb0000;
			ao->o |= (getreg (ao->a[0]));
			ao->o |= (getreg (ao->a[1])) << 24;
			ao->o |= (getreg (ao->a[2])) << 8;
			if (m & S_BIT) {
				ao->o |= 1 << 28;
			}
			return 4;
		        }
			break;
		case 0x311: {
			ao->a[3] = ao->a[2];
			ao->a[2] = ao->a[1];
			ao->a[1] = ao->a[0];
		        }
			// intentional fallthrough
			// a bit naughty, perhaps?
		case 0x3111: {
			if (m & DOTN_BIT) {
				// this is explicitly an error
				return -1;
			}
			ao->o = 0x40eb0000;
			ao->o |= (getreg (ao->a[0]));
			ao->o |= (getreg (ao->a[1])) << 24;
			ao->o |= (getreg (ao->a[2])) << 8;
			ao->o |= thumb_getshift (ao->a[3]);
			if (m & S_BIT) {
				ao->o |= 1 << 28;
			}
			return 4;
		        }
			break;
		default:
			return -1;
		}
	} else		
	if (!strcmpnull (ao->op, "sub")) {
		int reg = getreg (ao->a[1]);
		if (reg != -1) {
			int n = getnum (ao->a[2]); // TODO: add limit
			ao->o = 0x1e;
			ao->o |= (getreg (ao->a[0])) << 8;
			ao->o |= reg << 11;
			ao->o |= n / 4 | ((n % 4) << 14);
		} else {
			if (reg < 10) {
				int num = getnum (ao->a[1]);
				if (err) {
					return 0;
				}
				if (getreg (ao->a[0]) == 13 &&
				    (!(num & 0xb) || !(num & 0x7))) {
					ao->o = 0x80b0;
					ao->o |= num << 6;
					return 2;
				}
				if (num > 0xff) {
					if (num % 2) {
						return 0;
					}
					ao->o = 0xa0f58070;
					ao->o |= (num >> 8) << 16;
					ao->o |= (num & 0xff) << 7;
				} else {
					ao->o = 0xa0f10000;
					ao->o |= num << 8;
				}
				ao->o |= getreg (ao->a[0]) << 24;
				ao->o |= getreg (ao->a[0]);
			}
			/*ao->o = 0x30;
			ao->o |= 8 + (getreg (ao->a[0]));
			ao->o |= (getnum (ao->a[1]) & 0xff) << 8;*/
		}
		return 2;
	} else
	if (!strcmpnull(ao->op, "lsls") || !strcmpnull(ao->op, "lsrs")) {
		ut16 opcode = 0;	//0000 xiii iiMM MDDD; ls<x>s Rd, Rm, imm5
		if (ao->op[2] == 'r') opcode=0x0800;	//lsrs
		int Rd = getreg (ao->a[0]);
		int Rm = getreg (ao->a[1]);
		int a2 = getnum (ao->a[2]);
		if ((Rd < 0) || (Rd > 7) || (Rm < 0) || (Rm > 7) ||
				(a2 <= 0) || (a2 > 32)) {
			eprintf("illegal shift\n");	//bad regs, or imm5 out of range
			return 0;
		}
		opcode |= a2 << 6;
		opcode |= Rm << 3;
		opcode |= Rd;
		ao->o = (opcode >> 8) | ((opcode & 0xff) << 8);
		return 2;
	} else
	if (!strcmpnull (ao->op, "lsr")) {
		ao->o = 0x4fea1000;
		int Rd = getreg (ao->a[0]);
		int Rm = getreg (ao->a[1]);
		int a2 = getnum (ao->a[2]);
		if ((Rd < 0) || (Rd > 14) || (Rm < 0) || (Rm > 14) ||
				(a2 <= 0) || (a2 > 32)) {
			return 0;
		}
		ao->o |= Rd;
		ao->o |= Rm << 8;
		if (a2 == 32) {
			a2 = 0;
		}
		int a2b = a2 & 3;
		ao->o |= a2b << 14;
		int a2h = a2 >> 2;
		ao->o |= a2h << 4;

		return 4;
	}
	return 0;
}

static int findyz(int x, int *y, int *z) {
	int i, j;
	for (i = 0;i < 0xff; i++) {
		for (j = 0;j < 0xf;j++) {
			int v = i << j;
			if (v > x) continue;
			if (v == x) {
				*y = i;
				*z = 16 - (j / 2);
				return 1;
			}
		}
	}
	return 0;
}

static int arm_assemble(ArmOpcode *ao, ut64 off, const char *str) {
	int i, j, ret, reg, a, b;
	int coproc, opc;
	bool rex = false;
	int shift, low, high;
	for (i = 0; ops[i].name; i++) {
		if (!strncmp (ao->op, ops[i].name, strlen (ops[i].name))) {
			ao->o = ops[i].code;
			arm_opcode_cond (ao, strlen(ops[i].name));
			if (ao->a[0] || ops[i].type == TYPE_BKP)
			switch (ops[i].type) {
			case TYPE_MEM:
				if (!strncmp (ops[i].name, "strex", 5)) {
					rex = 1;
				}
				getrange (ao->a[0]);
				getrange (ao->a[1]);
				getrange (ao->a[2]);
				if (ao->a[0] && ao->a[1]) {
					char rn[8];
					strncpy (rn, ao->a[1], 7);
					int r0 = getreg (ao->a[0]);
					int r1 = getreg (ao->a[1]);
					if ( (r0 < 0 || r0 > 15) || (r1 > 15 || r1 < 0) ) {
						return 0;
					}
					ao->o |= r0 << 20;
					if (!strcmp (ops[i].name, "strd")) {
						r1 = getreg (ao->a[2]);
						if (r1 == -1) {
							break;
						}
						ao->o |= r1 << 8;
						if (ao->a[3]) {
							char *bracket = strchr (ao->a[3], ']');
							if (bracket) {
								*bracket = '\0';
							}
							int num = getnum (ao->a[3]);
							ao->o |= (num & 0x0f) << 24;
							ao->o |= ((num >> 4) & 0x0f) << 16;
						}
						break;
					}
					if (!strcmp (ops[i].name, "strh")) {
						ao->o |=  r1 << 8;
						if (ao->a[2]) {
							reg = getreg (ao->a[2]);
							if (reg != -1) {
								ao->o |= reg << 24;
							} else {
								ao->o |= 1 << 14;
								ao->o |= getnum (ao->a[2]) << 24;
							}
						} else {
							ao->o |= 1 << 14;
						}
						break;
					}
					if (rex) {
						ao->o |=  r1 << 24;
					} else {
						ao->o |=  r1 << 8; // delta
					}
				} else {
					return 0;
				}

				ret = getreg (ao->a[2]);
				if (ret != -1) {
					if (rex) {
						ao->o |= 1;
						ao->o |= (ret & 0x0f) << 8;
					} else {
						ao->o |= (strstr (str,"],")) ? 6 : 7;
						ao->o |= (ret & 0x0f) << 24;
					}
					if (ao->a[3]) {
						shift = getshift (ao->a[3]);
						low = shift & 0xFF;
						high = shift & 0xFF00;
						ao->o |= low << 24;
						ao->o |= high << 8;
					}
				} else {
					int num = getnum (ao->a[2]) & 0xfff;
					if (err) {
						break;
					}
					if (rex) {
						ao->o |= 1;
					} else {
						ao->o |= (strstr (str, "],")) ? 4 : 5;
					}
					ao->o |= 1;
					ao->o |= (num & 0xff) << 24;
					ao->o |= ((num >> 8) & 0xf) << 16;
				}

				break;
			case TYPE_IMM:
				if (*ao->a[0] ++== '{') {
					for (j = 0; j < 16; j++) {
						if (ao->a[j] && *ao->a[j]) {
							getrange (ao->a[j]); // XXX filter regname string
							reg = getreg (ao->a[j]);
							if (reg != -1) {
								if (reg < 8)
									ao->o |= 1 << (24 + reg);
								else
									ao->o |= 1 << (8 + reg);
							}
						}
					}
				} else ao->o |= getnum (ao->a[0]) << 24; // ???
				break;
			case TYPE_BRA:
				if ((ret = getreg (ao->a[0])) == -1) {
					// TODO: control if branch out of range
					ret = (getnum (ao->a[0]) - (int)ao->off - 8) / 4;
					if (ret >= 0x00800000 || ret < (int)0xff800000) {
						eprintf("Branch into out of range\n");
						return 0;
					}
					ao->o |= ((ret >> 16) & 0xff) << 8;
					ao->o |= ((ret >> 8) & 0xff) << 16;
					ao->o |= ((ret) & 0xff) << 24;
				} else {
					eprintf("This branch does not accept reg as arg\n");
					return 0;
				}
				break;
			case TYPE_BKP:
				ao->o |= 0x70 << 24;
				if (ao->a[0]) {
					int n = getnum (ao->a[0]);
					ao->o |= ((n & 0xf) << 24);
					ao->o |= (((n >> 4) & 0xff) << 16);
				}
				break;
			case TYPE_BRR:
				if ((ret = getreg (ao->a[0])) == -1) {
					ut32 dst = getnum (ao->a[0]);
					dst -= (ao->off + 8);
					if (dst & 0x2) {
						ao->o = 0xfb;
					} else {
						ao->o = 0xfa;
					}
					dst /= 4;
					ao->o |= ((dst >> 16) & 0xff) << 8;
					ao->o |= ((dst >> 8) & 0xff) << 16;
					ao->o |= ((dst) & 0xff) << 24;
					return 4;
				} else ao->o |= (getreg (ao->a[0]) << 24);
				break;
			case TYPE_HLT:
				{
					ut32 o = 0, n = getnum (ao->a[0]);
					o |= ((n >> 12) & 0xf) << 8;
					o |= ((n >> 8) & 0xf) << 20;
					o |= ((n >> 4) & 0xf) << 16;
					o |= ((n) & 0xf) << 24;
					ao->o |=o;
				}
				break;
			case TYPE_SWI:
				ao->o |= (getnum (ao->a[0]) & 0xff) << 24;
				ao->o |= ((getnum (ao->a[0]) >> 8) & 0xff) << 16;
				ao->o |= ((getnum (ao->a[0]) >> 16) & 0xff) << 8;
				break;
			case TYPE_UDF:
				{
					// e7f000f0 = udf 0
					// e7ffffff = udf 0xffff
					ut32 n = getnum (ao->a[0]);
					ao->o |= 0xe7;
					ao->o |= (n & 0xf) << 24;
					ao->o |= ((n >> 4) & 0xff) << 16;
					ao->o |= ((n >> 12) & 0xf) << 8;
				}
				break;
			case TYPE_ARI:
				if (!ao->a[2]) {
					ao->a[2] = ao->a[1];
					ao->a[1] = ao->a[0];
				}
				reg = getreg (ao->a[0]);
				if (reg == -1) {
					return 0;
				}
				ao->o |= reg << 20;

				reg = getreg (ao->a[1]);
				if (reg == -1) {
					return 0;
				}
				ao->o |= reg << 8;
				reg = getreg (ao->a[2]);
				ao->o |= (reg != -1)? reg << 24 : 2 | getnum (ao->a[2]) << 24;
				if (ao->a[3]) {
					ao->o |= getshift (ao->a[3]);
				}
				break;
			case TYPE_SWP:
				{
				int a1 = getreg (ao->a[1]);
				if (a1) {
					ao->o = 0xe1;
					ao->o |= (getreg (ao->a[0]) << 4) << 16;
					ao->o |= (0x90 + a1) << 24;
					if (ao->a[2]) {
						ao->o |= (getreg (ao->a[2] + 1)) << 8;
					} else {
						return 0;
					}
				}
				if (0xff == ((ao->o >> 16) & 0xff))
					return 0;
				}
				break;
			case TYPE_MOV:
				if (!strcmpnull (ao->op, "movs")) {
					ao->o = 0xb0e1;
				}
				ao->o |= getreg (ao->a[0]) << 20;
				ret = getreg (ao->a[1]);
				if (ret != -1) {
					ao->o |= ret << 24;
				} else {
					int immed = getimmed8 (ao->a[1]);
					if (err) {
						return 0;
					}
					ao->o |= 0xa003 | (immed & 0xff) << 24 | (immed >> 8) << 16;
				}
				break;
			case TYPE_MOVW:
				reg = getreg (ao->a[0]);
				if (reg == -1) {
					return 0;
				}
				ao->o |= getreg (ao->a[0]) << 20;
				ret = getnum (ao->a[1]);

				ao->o |= 0x3 | ret << 24;
				ao->o |= (ret & 0xf000) >> 4;
				ao->o |= (ret & 0xf00) << 8;
				break;
			case TYPE_MOVT:
				ao->o |= getreg (ao->a[0]) << 20;
				ret = getnum (ao->a[1]);

				ao->o |= 0x4003 | ret << 24;
				ao->o |= (ret & 0xf000) >> 4;
				ao->o |= (ret & 0xf00) << 8;
				break;
			case TYPE_MUL:
				if (!strcmpnull (ao->op, "mul")) {
					ret = getreg (ao->a[0]);
					a = getreg (ao->a[1]);
					b = getreg (ao->a[2]);
					if (b == -1) {
						b = a;
						a = ret;
					}
					if (ret == -1 || a == -1) {
						return 0;
					}
					ao->o |= ret << 8;
					ao->o |= a << 24;
					ao->o |= b << 16;
				} else {
					low = getreg (ao->a[0]);
					high = getreg (ao->a[1]);
					a = getreg (ao->a[2]);
					b = getreg (ao->a[3]);
					if (low == -1 || high == -1 || a == -1 || b == -1) {
						return 0;
					}
					if (!strcmpnull (ao->op, "smlal")) {
						ao->o |= low << 20;
						ao->o |= high << 8;
						ao->o |= a << 24;
						ao->o |= b << 16;
					} else if (!strncmp (ao->op, "smla", 4)) {
						if (low > 14 || high > 14 || a > 14) {
							return 0;
						}
						ao->o |= low << 8;
						ao->o |= high << 24;
						ao->o |= a << 16;
						ao->o |= b << 20;
						break;
					} else {
						ao->o |= low << 20;
						ao->o |= high << 8;
						ao->o |= a << 24;
						ao->o |= b << 16;
					}
				}
				break;
			case TYPE_TST:
				a = getreg (ao->a[0]);
				b = getreg (ao->a[1]);
				if (b == -1) {
					int y, z;
					b = getnum (ao->a[1]);
					if (b >= 0 && b <= 0xff) {
						ao->o = 0x50e3;
						// TODO: if (b>255) -> automatic multiplier
						ao->o |= (a << 8);
						ao->o |= ((b & 0xff) << 24);
					} else
					if (findyz (b, &y, &z)) {
						ao->o = 0x50e3;
						ao->o |= (y << 24);
						ao->o |= (z << 16);
					} else {
						eprintf ("Parameter %d out0x3000a0e1 of range (0-255)\n", (int)b);
						return 0;
					}
				} else {
					ao->o |= (a << 8);
					ao->o |= (b << 24);
					if (ao->a[2])
						ao->o |= getshift (ao->a[2]);
				}
				if (ao->a[2]) {
					int n = getnum (ao->a[2]);
					if (n & 1) {
						eprintf ("Invalid multiplier\n");
						return 0;
					}
					ao->o |= (n >> 1) << 16;
				}
				break;
			case TYPE_SHFT:
				reg = getreg (ao->a[2]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 16;

				reg = getreg (ao->a[0]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 20;

				reg = getreg (ao->a[1]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 24;
				break;
			case TYPE_REV:
				reg = getreg (ao->a[0]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 20;

				reg = getreg (ao->a[1]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 24;

				break;
			case TYPE_ENDIAN:
				if (!strcmp (ao->a[0], "le")) {
					ao->o |= 0;
				} else if (!strcmp (ao->a[0], "be")) {
					ao->o |= 0x20000;
				} else {
					return 0;
				}
				break;
			case TYPE_COPROC:
				//printf ("%s %s %s %s %s\n", ao->a[0], ao->a[1], ao->a[2], ao->a[3], ao->a[4] );
				if (ao->a[0]) {
					coproc = getnum (ao->a[0] + 1);
					if (coproc == -1 || coproc > 9) {
						return 0;
					}
					ao->o |= coproc << 16;
				}

				opc = getnum (ao->a[1]);
				if (opc == -1 || opc > 7) {
					return 0;
				}
				ao->o |= opc << 13;

				reg = getreg (ao->a[2]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 20;

				// coproc register 1
				const char *a3 = ao->a[3];
				if (a3) {
					coproc = getnum (a3 + 1);
					if (coproc == -1 || coproc > 15) {
						return 0;
					}
					ao->o |= coproc << 8;
				}

				const char *a4 = ao->a[4];
				if (a4) {
					coproc = getnum (ao->a[4] + 1);
					if (coproc == -1 || coproc > 15) {
						return 0;
					}
					ao->o |= coproc << 24;
				}

				coproc = getnum (ao->a[5]);
				if (coproc > -1) {
					if (coproc > 7) {
						return 0;
					}
					// optional opcode
					ao->o |= coproc << 29;
				}

				break;
			case TYPE_CLZ:
				ao->o |= 1 << 28;

				reg = getreg (ao->a[0]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 20;

				reg = getreg (ao->a[1]);
				if (reg == -1 || reg > 14) {
					return 0;
				}
				ao->o |= reg << 24;

				break;
			}
			return 1;
		}
	}
	return 0;
}

typedef int (*AssembleFunction)(ArmOpcode *, ut64, const char *);
static AssembleFunction assemble[2] = { &arm_assemble, &thumb_assemble };

ut32 armass_assemble(const char *str, ut64 off, int thumb) {
	int i, j;
	char buf[128];
	ArmOpcode aop = {.off = off};
	for (i = j = 0; i < sizeof (buf) - 1 && str[i]; i++, j++) {
		if (str[j] == '#') {
			i--; continue;
		}
		buf[i] = tolower ((const ut8)str[j]);
	}
	buf[i] = 0;
	arm_opcode_parse (&aop, buf);
	aop.off = off;
	if (thumb < 0 || thumb > 1) {
		return -1;
	}
	if (!assemble[thumb] (&aop, off, buf)) {
		//eprintf ("armass: Unknown opcode (%s)\n", buf);
		return -1;
	}
	return aop.o;
}

#ifdef MAIN
void thisplay(const char *str) {
	char cmd[32];
	int op = armass_assemble (str, 0x1000, 1);
	printf ("[%04x] %s\n", op, str);
	snprintf (cmd, sizeof(cmd), "rasm2 -d -b 16 -a arm %04x", op);
	system (cmd);
}

void display(const char *str) {
	char cmd[32];
	int op = armass_assemble (str, 0x1000, 0);
	printf ("[%08x] %s\n", op, str);
	snprintf (cmd, sizeof(cmd), "rasm2 -d -a arm %08x", op);
	system (cmd);
}

int main() {
	thisplay ("ldmia r1!, {r3, r4, r5}");
	thisplay ("stmia r1!, {r3, r4, r5}");
	thisplay ("bkpt 12");
return 0;
	thisplay("sub r1, r2, 0");
	thisplay("sub r1, r2, 4");
	thisplay("sub r1, r2, 5");
	thisplay("sub r1, r2, 7");
	thisplay("sub r3, 44");
return 0;
#if 0
	thisplay("mov r0, 11");
	thisplay("mov r0, r2");
	thisplay("mov r1, r4");
	thisplay("cmp r1, r2");
	thisplay("cmp r3, 44");
	thisplay("nop");
	thisplay("svc 15");
	thisplay("add r1, r2");
	thisplay("add r3, 44");
	thisplay("sub r1, r2, 3");
	thisplay("sub r3, 44");
	thisplay("tst r3,r4");
	thisplay("bx r3");
	thisplay("b 33");
	thisplay("b 0");
	thisplay("bne 44");
	thisplay("and r2,r3");
#endif
	// INVALID thisplay("ldr r1, [pc, r2]");
	// INVALID thisplay("ldr r1, [sp, r2]");
#if 0
	thisplay("ldr r1, [pc, 12]");
	thisplay("ldr r1, [sp, 24]");
	thisplay("ldr r1, [r2, r3]");
#endif
	// INVALID thisplay("str r1, [pc, 22]");
	// INVALID thisplay("str r1, [pc, r2]");
	// INVALID thisplay("str r1, [sp, r2]");
#if 0
   0:   8991            ldrh    r1, [r2, #12]
   2:   7b11            ldrb    r1, [r2, #12]
   4:   8191            strh    r1, [r2, #12]
   6:   7311            strb    r1, [r2, #12]
#endif
	thisplay("ldrh r1, [r2, 8]"); // aligned to 4
	thisplay("ldrh r1, [r3, 8]"); // aligned to 4
	thisplay("ldrh r1, [r4, 16]"); // aligned to 4
	thisplay("ldrh r1, [r2, 32]"); // aligned to 4
	thisplay("ldrb r1, [r2, 20]"); // aligned to 4
	thisplay("strh r1, [r2, 20]"); // aligned to 4
	thisplay("strb r1, [r2, 20]"); // aligned to 4
	thisplay("str r1, [sp, 20]"); // aligned to 4
	thisplay("str r1, [r2, 12]"); // OK
	thisplay("str r1, [r2, r3]");
return 0;
#if 0
	display("mov r0, 33");
	display("mov r1, 33");
	display("movne r0, 33");
	display("tst r0, r1, lsl #2");
	display("svc 0x80");
	display("sub r3, r1, r2");
	display("add r0, r1, r2");
	display("mov fp, 0");
	display("pop {pc}");
	display("pop {r3}");
	display("bx r1");
	display("bx r3");
	display("bx pc");
	display("blx fp");
	display("pop {pc}");
	display("add lr, pc, lr");
	display("adds r3, #8");
	display("adds r3, r2, #8");
	display("subs r2, #1");
	display("cmp r0, r4");
	display("cmp r7, pc");
	display("cmp r1, r3");
	display("mov pc, 44");
	display("mov pc, r3");
	display("push {pc}");
	display("pop {pc}");
	display("nop");
	display("ldr r1, [r2, 33]");
	display("ldr r1, [r2, r3]");
	display("ldr r3, [r4, r6]");
	display("str r1, [pc, 33]");
	display("str r1, [pc], 2");
	display("str r1, [pc, 3]");
	display("str r1, [pc, r4]");
	display("bx r3");
	display("bcc 33");
	display("blx r3");
	display("bne 0x1200");
	display("str r0, [r1]");
	display("push {fp,lr}");
	display("pop {fp,lr}");
	display("pop {pc}");
#endif

   //10ab4:       00047e30        andeq   r7, r4, r0, lsr lr
   //10ab8:       00036e70        andeq   r6, r3, r0, ror lr

	display("andeq r7, r4, r0, lsr lr");
	display("andeq r6, r3, r0, ror lr");
//  c4:   e8bd80f0        pop     {r4, r5, r6, r7, pc}
	display("pop {r4,r5,r6,r7,pc}");


#if 0
	display("blx r1");
	display("blx 0x8048");
#endif

#if 0
	display("b 0x123");
	display("bl 0x123");
	display("blt 0x123"); // XXX: not supported
#endif
	return 0;
}
#endif
