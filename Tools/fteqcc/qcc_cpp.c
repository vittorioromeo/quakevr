// qcc_cpp.c -- Quake VR's C++ backend for FTEQCC (docs/vr-port/QC_TO_CPP.md). GPL-2.0-or-later, as FTEQCC.
//
// "-cppout <dir>" makes the compile also write the progs it just compiled as C++: one .cpp per QuakeC source file
// (its functions, in source order), qc_funcs.h (every function's declaration) and qc_table.cpp (the function table by
// progs index with the names, the field table, the progs hash the engine checks before it runs any of it).
//
// It works from the compiler's own statements (the same ones progs.dat gets, after every optimisation) with their
// symbols, so globals, locals, fields and functions keep their QuakeC names, and each line of QuakeC is a comment
// above its C++. Every statement becomes exactly what the engine's interpreter does for it (pr_exec.c), on the same
// memory (the VM's globals and edicts, through the macros of Quake/vr/qccpp/qccpp_rt.h): saves, MP and the engine's
// own reads of fields and globals see no difference, and vr_qc_cpp 0/1 can switch at any frame. Calls between
// QuakeC functions are direct C++ calls; builtins go through the engine's builtin table as the VM's calls do.
//
// Hooked into QCC_WriteData (qccmain.c, Tools/fteqcc/cpp.patch): QCC_CppGen_Maybe before the globals are written.

#include "qcc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

void QCC_CppGen_Maybe (const QCC_dstatement32_t *st, const dfunction_t *df);

static const QCC_dstatement32_t *cg_st;
static const dfunction_t *cg_df;
static char cg_dir[1024];
static int cg_errors;

// ---- small growable text buffer ----
typedef struct { char *p; size_t n, cap; } cgbuf_t;
static void cb_putn (cgbuf_t *b, const char *s, size_t n)
{
	if (b->n + n + 1 > b->cap)
	{
		size_t c = b->cap ? b->cap * 2 : 65536;
		while (c < b->n + n + 1)
			c *= 2;
		b->p = (char *)realloc (b->p, c);
		b->cap = c;
	}
	memcpy (b->p + b->n, s, n);
	b->n += n;
	b->p[b->n] = 0;
}
static void cb_puts (cgbuf_t *b, const char *s) { cb_putn (b, s, strlen (s)); }
static void cb_printf (cgbuf_t *b, const char *fmt, ...)
{
	char tmp[4096];
	va_list va;
	int n;
	va_start (va, fmt);
	n = vsnprintf (tmp, sizeof (tmp), fmt, va);
	va_end (va);
	if (n < 0)
		return;
	if ((size_t)n >= sizeof (tmp))
		n = sizeof (tmp) - 1;
	cb_putn (b, tmp, (size_t)n);
}

// ---- names ----
static const char *const cg_keywords[] = {
	"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char",
	"class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
	"co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
	"export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace",
	"new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
	"register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
	"static_cast", "struct", "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid",
	"typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
	"main", "NULL", "errno", "assert", "min", "max", "F", "I", "V", "qg", NULL};

// A usable name for a symbol, or 0 (temps, immediates, compiler-made names).
static int cg_named (const char *n)
{
	if (!n || !*n || !strcmp (n, "IMMEDIATE"))
		return 0;
	if (!((n[0] >= 'a' && n[0] <= 'z') || (n[0] >= 'A' && n[0] <= 'Z') || n[0] == '_'))
		return 0;
	if (!strncmp (n, "tmp", 3) || !strncmp (n, "temp", 4) || !strncmp (n, "<", 1))
		return 0;
	return 1;
}

static void cg_sanitize (const char *n, char *out, size_t outsz)
{
	size_t i = 0, j;
	for (; *n && i + 2 < outsz; n++)
	{
		char c = *n;
		out[i++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') ? c : '_';
	}
	out[i] = 0;
	if (i && out[i - 1] == '_' && i >= 2 && out[i - 2] == '_')
		; // (double underscores are reserved in C++, but these names stay inside namespaces: harmless)
	for (j = 0; cg_keywords[j]; j++)
		if (!strcmp (out, cg_keywords[j]))
		{
			if (i + 2 < outsz)
				out[i] = '_', out[i + 1] = 0;
			break;
		}
}

// Name sets: one name per offset (the first name seen for it); another offset with a name already taken gets
// "<name>_<ofs>". Hashed both ways (offset -> entry, name -> taken): millions of operands look up here.
typedef struct { char *name; unsigned ofs; } cgname_t;
typedef struct { cgname_t *e; int n, cap; int *byofs, *byname; int hcap; } cgnames_t;

static unsigned cg_strhash (const char *s)
{
	unsigned h = 2166136261u;
	for (; *s; s++)
		h = (h ^ (unsigned char)*s) * 16777619u;
	return h;
}
static void cg_nameset_rehash (cgnames_t *s, int hcap)
{
	int i;
	free (s->byofs);
	free (s->byname);
	s->hcap = hcap;
	s->byofs = (int *)malloc (sizeof (int) * hcap);
	s->byname = (int *)malloc (sizeof (int) * hcap);
	memset (s->byofs, -1, sizeof (int) * hcap);
	memset (s->byname, -1, sizeof (int) * hcap);
	for (i = 0; i < s->n; i++)
	{
		unsigned h = (s->e[i].ofs * 2654435761u) & (hcap - 1);
		while (s->byofs[h] >= 0)
			h = (h + 1) & (hcap - 1);
		s->byofs[h] = i;
		h = cg_strhash (s->e[i].name) & (hcap - 1);
		while (s->byname[h] >= 0)
			h = (h + 1) & (hcap - 1);
		s->byname[h] = i;
	}
}
static int cg_nameset_taken (const cgnames_t *s, const char *name)
{
	unsigned h;
	if (!s->hcap)
		return 0;
	for (h = cg_strhash (name) & (s->hcap - 1); s->byname[h] >= 0; h = (h + 1) & (s->hcap - 1))
		if (!strcmp (s->e[s->byname[h]].name, name))
			return 1;
	return 0;
}

static const char *cg_nameset_get (cgnames_t *s, const char *raw, unsigned ofs)
{
	char base[256], cand[300];
	unsigned h;
	if (s->hcap)
		for (h = (ofs * 2654435761u) & (s->hcap - 1); s->byofs[h] >= 0; h = (h + 1) & (s->hcap - 1))
			if (s->e[s->byofs[h]].ofs == ofs)
				return s->e[s->byofs[h]].name;
	cg_sanitize (raw, base, sizeof (base));
	if (cg_nameset_taken (s, base))
		snprintf (cand, sizeof (cand), "%s_%u", base, ofs);
	else
		snprintf (cand, sizeof (cand), "%s", base);
	if (s->n == s->cap)
	{
		s->cap = s->cap ? s->cap * 2 : 64;
		s->e = (cgname_t *)realloc (s->e, sizeof (cgname_t) * s->cap);
	}
	s->e[s->n].name = strdup (cand);
	s->e[s->n].ofs = ofs;
	s->n++;
	if (s->n * 2 > s->hcap)
		cg_nameset_rehash (s, s->hcap ? s->hcap * 2 : 128);
	else
	{
		const int i = s->n - 1;
		for (h = (ofs * 2654435761u) & (s->hcap - 1); s->byofs[h] >= 0; h = (h + 1) & (s->hcap - 1))
			;
		s->byofs[h] = i;
		for (h = cg_strhash (cand) & (s->hcap - 1); s->byname[h] >= 0; h = (h + 1) & (s->hcap - 1))
			;
		s->byname[h] = i;
	}
	return s->e[s->n - 1].name;
}
static void cg_nameset_clear (cgnames_t *s)
{
	int i;
	for (i = 0; i < s->n; i++)
		free (s->e[i].name);
	s->n = 0;
	if (s->hcap)
	{
		memset (s->byofs, -1, sizeof (int) * s->hcap);
		memset (s->byname, -1, sizeof (int) * s->hcap);
	}
}

static char **cg_fname; // C++ name of each function (index), NULL for builtins

// ---- source lines (each QuakeC line as a comment above its C++) ----
static char *cg_srcname, *cg_srcdata;
static char **cg_srclines;
static int cg_srcnum;

static void cg_loadsrc (const char *fname)
{
	FILE *f;
	long len;
	int i;
	if (cg_srcname && !strcmp (cg_srcname, fname))
		return;
	free (cg_srcname);
	free (cg_srcdata);
	free (cg_srclines);
	cg_srcname = strdup (fname);
	cg_srcdata = NULL;
	cg_srclines = NULL;
	cg_srcnum = 0;
	f = fopen (fname, "rb");
	if (!f)
		return;
	fseek (f, 0, SEEK_END);
	len = ftell (f);
	fseek (f, 0, SEEK_SET);
	cg_srcdata = (char *)malloc (len + 1);
	len = (long)fread (cg_srcdata, 1, len, f);
	cg_srcdata[len] = 0;
	fclose (f);
	for (i = 0; i < len; i++)
		if (cg_srcdata[i] == '\n')
			cg_srcnum++;
	cg_srclines = (char **)malloc (sizeof (char *) * (cg_srcnum + 2));
	cg_srcnum = 0;
	cg_srclines[cg_srcnum++] = cg_srcdata;
	for (i = 0; i < len; i++)
		if (cg_srcdata[i] == '\n')
		{
			cg_srcdata[i] = 0;
			cg_srclines[cg_srcnum++] = cg_srcdata + i + 1;
		}
}

static void cg_comment_text (cgbuf_t *b, const char *s, size_t maxlen)
{
	size_t n = 0, end;
	char c;
	while (*s == ' ' || *s == '\t')
		s++;
	end = strlen (s);
	while (end && (s[end - 1] == '\r' || s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\\'))
		end--;
	for (; n < end && n < maxlen; n++)
	{
		c = s[n];
		if ((unsigned char)c >= 0x80 || c < 0x20)
			c = c == '\t' ? ' ' : '?';
		if (c == '?' && n + 1 < end && s[n + 1] == '?')
			c = '.'; // no trigraph
		if (c == '*' && n + 1 < end && s[n + 1] == '/')
			c = '.';
		cb_putn (b, &c, 1);
	}
}

// ---- operands ----
static cgnames_t cg_gnames, cg_lnames, cg_fldnames; // per file, per function, per file
static cgbuf_t cg_ldecl; // the current function's local constants

static const QCC_sref_t *cg_sref (int s, int which)
{
	return which == 0 ? &statements[s].a : which == 1 ? &statements[s].b : &statements[s].c;
}
static unsigned cg_ofs (int s, int which)
{
	return which == 0 ? cg_st[s].a : which == 1 ? cg_st[s].b : cg_st[s].c;
}

// The offset expression of operand which of statement s ("g::time", "l_dist + 1", "1234").
static void cg_ofsexpr (cgbuf_t *b, int s, int which)
{
	const QCC_sref_t *r = cg_sref (s, which);
	unsigned ofs = cg_ofs (s, which);
	const QCC_def_t *d = r->sym;
	if (d && cg_named (d->name) && d->ofs + r->ofs == ofs)
	{
		const char *n;
		if (d->scope && !d->isstatic)
		{
			int before = cg_lnames.n;
			char tmp[300];
			snprintf (tmp, sizeof (tmp), "l_%s", d->name);
			n = cg_nameset_get (&cg_lnames, tmp, d->ofs);
			if (cg_lnames.n != before)
				cb_printf (&cg_ldecl, "\tconstexpr int %s = %u;\n", n, d->ofs);
			cb_puts (b, n);
		}
		else
		{
			n = cg_nameset_get (&cg_gnames, d->name, d->ofs);
			cb_printf (b, "g::%s", n);
		}
		if (r->ofs)
			cb_printf (b, " + %u", r->ofs);
		return;
	}
	cb_printf (b, "%u", ofs);
}

static int cg_is_immediate (int s, int which)
{
	const QCC_sref_t *r = cg_sref (s, which);
	return r->sym && r->sym->name && !strcmp (r->sym->name, "IMMEDIATE");
}

// A float literal that is exactly the bits of v, or 0 if there is none (NaN, infinity).
static int cg_floatlit (float v, char *out, size_t outsz)
{
	unsigned bits, bits2;
	float back;
	memcpy (&bits, &v, 4);
	if (!isfinite (v))
		return 0;
	snprintf (out, outsz, "%.9g", v);
	back = strtof (out, NULL);
	memcpy (&bits2, &back, 4);
	if (bits != bits2)
		return 0;
	if (!strchr (out, '.') && !strchr (out, 'e') && !strchr (out, 'E'))
		strncat (out, ".0", outsz - strlen (out) - 1);
	strncat (out, "f", outsz - strlen (out) - 1);
	return 1;
}

// A float operand as an rvalue or lvalue: "F(g::time)", or a literal for an immediate read.
static void cg_f (cgbuf_t *b, int s, int which, int rvalue)
{
	char lit[64];
	if (rvalue && cg_is_immediate (s, which))
	{
		float v = qcc_pr_globals[cg_ofs (s, which)]._float;
		if (cg_floatlit (v, lit, sizeof (lit)))
		{
			cb_puts (b, lit);
			return;
		}
	}
	cb_puts (b, "F(");
	cg_ofsexpr (b, s, which);
	cb_puts (b, ")");
}
static void cg_i (cgbuf_t *b, int s, int which)
{
	cb_puts (b, "I(");
	cg_ofsexpr (b, s, which);
	cb_puts (b, ")");
}

// A field operand (OP_LOAD_*/OP_ADDRESS's b): the field's offset as a constant when the global is the field's def.
static void cg_field (cgbuf_t *b, int s)
{
	const QCC_sref_t *r = cg_sref (s, 1);
	const QCC_def_t *d = r->sym;
	unsigned ofs = cg_ofs (s, 1);
	if (d && d->constant && cg_named (d->name) && d->type && d->type->type == ev_field && !r->ofs && d->ofs == ofs)
	{
		const char *n = cg_nameset_get (&cg_fldnames, d->name, (unsigned)qcc_pr_globals[ofs]._int);
		cb_printf (b, "fld::%s", n);
		return;
	}
	cg_i (b, s, 1);
}

// A comment showing an immediate string or vector (the C++ reads it from the globals, as the VM does).
static void cg_immcomment (cgbuf_t *b, int s, int which, int op)
{
	unsigned ofs = cg_ofs (s, which);
	if (!cg_is_immediate (s, which))
		return;
	if (op == 12 || op == 17 || op == 33 || op == 39 || op == 46)
	{
		int so = qcc_pr_globals[ofs].string;
		if (so > 0 && so < strofs)
		{
			cb_puts (b, " // \"");
			cg_comment_text (b, strings + so, 60);
			cb_puts (b, "\"");
		}
	}
	else if (op == 7 || op == 9 || op == 11 || op == 16 || op == 32 || op == 38 || op == 45)
		cb_printf (b, " // '%g %g %g'", qcc_pr_globals[ofs]._float, qcc_pr_globals[ofs + 1]._float,
				   qcc_pr_globals[ofs + 2]._float);
}

// The function a constant call operand names, or -1.
static int cg_callee (int s)
{
	const QCC_sref_t *r = cg_sref (s, 0);
	const QCC_def_t *d = r->sym;
	unsigned ofs = cg_ofs (s, 0);
	int fn;
	if (!d || !d->constant || !d->type || d->type->type != ev_function || r->ofs || d->ofs != ofs)
		return -1;
	if (d->scope && !d->isstatic)
		return -1;
	fn = qcc_pr_globals[ofs].function;
	if (fn <= 0 || fn >= numfunctions)
		return -1;
	return fn;
}

static const char *cg_fnname (int fn)
{
	return strings + cg_df[fn].s_name;
}

// ---- one statement ----
static void cg_statement (cgbuf_t *b, int s)
{
	const int op = (int)cg_st[s].op;
	const char *const binf[] = {NULL, "*", NULL, NULL, NULL, "/", "+", NULL, "-"};
	cb_puts (b, "\t");
	switch (op)
	{
	case 1: case 5: case 6: case 8: // MUL_F DIV_F ADD_F SUB_F
		cg_f (b, s, 2, 0); cb_puts (b, " = "); cg_f (b, s, 0, 1); cb_printf (b, " %s ", binf[op]); cg_f (b, s, 1, 1);
		cb_puts (b, ";");
		break;
	case 2: // MUL_V (dot)
		cg_f (b, s, 2, 0); cb_puts (b, " = QCPP_DOT("); cg_ofsexpr (b, s, 0); cb_puts (b, ", "); cg_ofsexpr (b, s, 1);
		cb_puts (b, ");");
		break;
	case 3: case 4: case 7: case 9: // MUL_FV MUL_VF ADD_V SUB_V
		cb_puts (b, op == 3 ? "QCPP_MULFV(" : op == 4 ? "QCPP_MULVF(" : op == 7 ? "QCPP_VADD(" : "QCPP_VSUB(");
		cg_ofsexpr (b, s, 2); cb_puts (b, ", "); cg_ofsexpr (b, s, 0); cb_puts (b, ", "); cg_ofsexpr (b, s, 1);
		cb_puts (b, ");");
		break;
	case 10: case 15: case 20: case 21: case 22: case 23: case 62: case 63: // EQ_F NE_F LE GE LT GT AND OR
	{
		const char *o = op == 10 ? "==" : op == 15 ? "!=" : op == 20 ? "<=" : op == 21 ? ">=" : op == 22 ? "<" :
						op == 23 ? ">" : op == 62 ? "&&" : "||";
		cg_f (b, s, 2, 0); cb_puts (b, " = "); cg_f (b, s, 0, 1); cb_printf (b, " %s ", o); cg_f (b, s, 1, 1);
		cb_puts (b, ";");
		break;
	}
	case 11: case 16: case 12: case 17: // EQ_V NE_V EQ_S NE_S
		cg_f (b, s, 2, 0);
		cb_puts (b, op == 11 ? " = QCPP_VEQ(" : op == 16 ? " = QCPP_VNE(" : op == 12 ? " = QCPP_SEQ(" : " = QCPP_SNE(");
		if (op == 11 || op == 16)
		{
			cg_ofsexpr (b, s, 0); cb_puts (b, ", "); cg_ofsexpr (b, s, 1);
		}
		else
		{
			cg_i (b, s, 0); cb_puts (b, ", "); cg_i (b, s, 1);
		}
		cb_puts (b, ");");
		cg_immcomment (b, s, 0, op);
		cg_immcomment (b, s, 1, op);
		break;
	case 13: case 14: case 18: case 19: // EQ_E EQ_FNC NE_E NE_FNC
		cg_f (b, s, 2, 0); cb_puts (b, " = "); cg_i (b, s, 0); cb_puts (b, op <= 14 ? " == " : " != "); cg_i (b, s, 1);
		cb_puts (b, ";");
		break;
	case 24: case 26: case 27: case 28: case 29: // LOAD_F LOAD_S LOAD_ENT LOAD_FLD LOAD_FNC
		cg_i (b, s, 2); cb_puts (b, " = QCPP_LOAD("); cg_i (b, s, 0); cb_puts (b, ", "); cg_field (b, s); cb_puts (b, ");");
		break;
	case 25: // LOAD_V
		cb_puts (b, "QCPP_LOADV("); cg_ofsexpr (b, s, 2); cb_puts (b, ", "); cg_i (b, s, 0); cb_puts (b, ", ");
		cg_field (b, s); cb_puts (b, ");");
		break;
	case 30: // ADDRESS
		cg_i (b, s, 2); cb_puts (b, " = QCPP_ADDRESS("); cg_i (b, s, 0); cb_puts (b, ", "); cg_field (b, s);
		cb_printf (b, ", %d);", s);
		break;
	case 31: case 33: case 34: case 35: case 36: // STORE_F STORE_S STORE_ENT STORE_FLD STORE_FNC (bit copies)
	{
		char lit[64];
		if (op == 31 && cg_is_immediate (s, 0) && cg_floatlit (qcc_pr_globals[cg_st[s].a]._float, lit, sizeof (lit)))
		{
			cg_f (b, s, 1, 0); cb_printf (b, " = %s;", lit);
		}
		else
		{
			cg_i (b, s, 1); cb_puts (b, " = "); cg_i (b, s, 0); cb_puts (b, ";");
		}
		cg_immcomment (b, s, 0, op);
		break;
	}
	case 32: // STORE_V
		cb_puts (b, "QCPP_VSTORE("); cg_ofsexpr (b, s, 1); cb_puts (b, ", "); cg_ofsexpr (b, s, 0); cb_puts (b, ");");
		cg_immcomment (b, s, 0, op);
		break;
	case 37: case 39: case 40: case 41: case 42: // STOREP_*
		cb_puts (b, "QCPP_STOREP("); cg_i (b, s, 1); cb_puts (b, ", "); cg_i (b, s, 0); cb_puts (b, ");");
		cg_immcomment (b, s, 0, op);
		break;
	case 38: // STOREP_V
		cb_puts (b, "QCPP_STOREPV("); cg_i (b, s, 1); cb_puts (b, ", "); cg_ofsexpr (b, s, 0); cb_puts (b, ");");
		cg_immcomment (b, s, 0, op);
		break;
	case 0: case 43: // DONE RETURN
		cb_puts (b, "QCPP_RETURN("); cg_ofsexpr (b, s, 0); cb_printf (b, ", %d); return;", s);
		break;
	case 44: // NOT_F
		cg_f (b, s, 2, 0); cb_puts (b, " = !"); cg_f (b, s, 0, 1); cb_puts (b, ";");
		break;
	case 45: // NOT_V
		cg_f (b, s, 2, 0); cb_puts (b, " = QCPP_VNOT("); cg_ofsexpr (b, s, 0); cb_puts (b, ");");
		break;
	case 46: case 47: case 48: // NOT_S NOT_ENT NOT_FNC
		cg_f (b, s, 2, 0); cb_puts (b, op == 46 ? " = QCPP_SNOT(" : op == 47 ? " = QCPP_ENOT(" : " = !(");
		cg_i (b, s, 0); cb_puts (b, ");");
		cg_immcomment (b, s, 0, op);
		break;
	case 49: case 50: // IF IFNOT (on the bits, as the VM)
		cb_puts (b, op == 49 ? "if (" : "if (!"); cg_i (b, s, 0); cb_printf (b, ") goto s%d;", s + (int)cg_st[s].b);
		break;
	case 61: // GOTO
		cb_printf (b, "goto s%d;", s + (int)cg_st[s].a);
		break;
	case 51: case 52: case 53: case 54: case 55: case 56: case 57: case 58: case 59: // CALL0-8
	{
		const int fn = cg_callee (s);
		if (fn > 0 && cg_df[fn].first_statement < 0)
			cb_printf (b, "QCPP_BUILTIN(%d, %d, %d); // %s", s, op - 51, fn, cg_fnname (fn));
		else if (fn > 0 && cg_fname[fn])
			cb_printf (b, "QCPP_CALL(%d, %d, qc::%s);", s, op - 51, cg_fname[fn]);
		else
		{
			cb_printf (b, "QCPP_CALLD(%d, %d, ", s, op - 51); cg_i (b, s, 0); cb_puts (b, ");");
		}
		break;
	}
	case 60: // STATE
		cb_puts (b, "QCPP_STATE("); cg_f (b, s, 0, 1); cb_puts (b, ", "); cg_i (b, s, 1); cb_puts (b, ");");
		break;
	case 64: case 65: // BITAND BITOR (memory operands: no compile-time float-to-int folding)
		cb_puts (b, "F("); cg_ofsexpr (b, s, 2); cb_puts (b, ") = (float)((int)F("); cg_ofsexpr (b, s, 0);
		cb_puts (b, op == 64 ? ") & (int)F(" : ") | (int)F("); cg_ofsexpr (b, s, 1); cb_puts (b, "));");
		break;
	default:
		externs->Printf ("qcc_cpp: statement %d: opcode %d has no C++ form\n", s, op);
		cg_errors++;
		cb_printf (b, "#error \"opcode %d\"", op);
	}
	cb_puts (b, "\n");
}

static int cg_isjump (int op) { return op == 49 || op == 50 || op == 61; }
static int cg_endsblock (int op) { return cg_isjump (op) || (op >= 51 && op <= 59) || op == 0 || op == 43; }

// ---- one function ----
static void cg_function (cgbuf_t *out, int fn, int first, int end, unsigned char *label)
{
	cgbuf_t body = {0};
	int s, lastline = -1, blockstart;
	const QCC_function_t *f = &functions[fn];
	const char *fname = f->filen ? f->filen : "";

	cg_nameset_clear (&cg_lnames);
	cg_ldecl.n = 0;
	if (cg_ldecl.p)
		cg_ldecl.p[0] = 0;

	for (s = first; s < end; s++)
		label[s] = 0;
	for (s = first; s < end; s++)
	{
		const int op = (int)cg_st[s].op;
		int t = -1;
		if (op == 49 || op == 50)
			t = s + (int)cg_st[s].b;
		else if (op == 61)
			t = s + (int)cg_st[s].a;
		if (t >= 0)
		{
			if (t < first || t >= end)
			{
				externs->Printf ("qcc_cpp: %s: jump out of the function at statement %d\n", cg_fnname (fn), s);
				cg_errors++;
			}
			else
				label[t] = 1;
		}
	}
	cg_loadsrc (fname);
	blockstart = 1;
	for (s = first; s < end; s++)
	{
		const int line = (int)statements[s].linenum;
		if (line != lastline && line > 0)
		{
			lastline = line;
			if (line <= cg_srcnum)
			{
				cb_puts (&body, "\t// ");
				cg_comment_text (&body, cg_srclines[line - 1], 160);
				cb_puts (&body, "\n");
			}
		}
		if (label[s])
		{
			cb_printf (&body, "s%d:\n", s);
			blockstart = 1;
		}
		if (blockstart)
		{
			int e = s + 1;
			while (e < end && !label[e] && !cg_endsblock ((int)cg_st[e - 1].op))
				e++;
			cb_printf (&body, "\tQCPP_COUNT(%d, %d);\n", e - s, s);
			blockstart = 0;
		}
		cg_statement (&body, s);
		if (cg_endsblock ((int)cg_st[s].op))
			blockstart = 1;
	}

	cb_printf (out, "\n// %s:%d\nvoid %s ()\n{\n\tQCPP_ENTER(%d);\n", fname, f->line, cg_fname[fn], fn);
	if (cg_ldecl.n)
		cb_puts (out, cg_ldecl.p);
	cb_putn (out, body.p, body.n);
	cb_puts (out, "}\n");
	free (body.p);
}

static void cg_writefile (const char *name, const cgbuf_t *b)
{
	char path[1200];
	FILE *f;
	snprintf (path, sizeof (path), "%s/%s", cg_dir, name);
	// unchanged files are left alone (their timestamps: MSBuild recompiles only what changed)
	f = fopen (path, "rb");
	if (f)
	{
		long len;
		fseek (f, 0, SEEK_END);
		len = ftell (f);
		if (len == (long)b->n)
		{
			char *old = (char *)malloc (len + 1);
			fseek (f, 0, SEEK_SET);
			if (fread (old, 1, len, f) == (size_t)len && !memcmp (old, b->p, len))
			{
				free (old);
				fclose (f);
				return;
			}
			free (old);
		}
		fclose (f);
	}
	f = fopen (path, "wb");
	if (!f)
	{
		externs->Printf ("qcc_cpp: can't write %s\n", path);
		cg_errors++;
		return;
	}
	fwrite (b->p, 1, b->n, f);
	fclose (f);
}

static unsigned cg_fnv (unsigned h, const void *p, size_t n)
{
	const unsigned char *c = (const unsigned char *)p;
	size_t i;
	for (i = 0; i < n; i++)
		h = (h ^ c[i]) * 16777619u;
	return h;
}

static int cg_cmp_start (const void *a, const void *b)
{
	const int fa = *(const int *)a, fb = *(const int *)b;
	return cg_df[fa].first_statement - cg_df[fb].first_statement;
}

void QCC_CppGen_Maybe (const QCC_dstatement32_t *st, const dfunction_t *df)
{
	int p = QCC_CheckParm ("-cppout"), i, j, nq = 0, nfiles = 0;
	int *order, *ends;
	unsigned char *label;
	cgnames_t fnames = {0};
	cgbuf_t hdr = {0}, tab = {0};
	unsigned hash = 2166136261u;
	const char *curfile = NULL;
	cgbuf_t file = {0}, funcs = {0};
	char outname[512];

	if (!p || p + 1 >= myargc)
		return;
	if (!st || !df)
	{
		externs->Printf ("qcc_cpp: -cppout needs the 32-bit progs format (fte32)\n");
		return;
	}
	snprintf (cg_dir, sizeof (cg_dir), "%s", myargv[p + 1]);
	cg_st = st;
	cg_df = df;
	cg_errors = 0;

	// the C++ name of each function
	cg_fname = (char **)calloc (numfunctions, sizeof (char *));
	for (i = 1; i < numfunctions; i++)
		if (df[i].first_statement > 0)
		{
			const char *raw = strings + df[i].s_name;
			cg_fname[i] = strdup (cg_nameset_get (&fnames, cg_named (raw) ? raw : "fn", (unsigned)i));
		}

	// functions in statement order; each runs to the next one's first statement
	order = (int *)malloc (sizeof (int) * numfunctions);
	ends = (int *)calloc (numfunctions, sizeof (int));
	for (i = 1; i < numfunctions; i++)
		if (df[i].first_statement > 0)
			order[nq++] = i;
	qsort (order, nq, sizeof (int), cg_cmp_start);
	for (i = 0; i < nq; i++)
		ends[order[i]] = i + 1 < nq ? df[order[i + 1]].first_statement : numstatements;
	label = (unsigned char *)calloc (numstatements + 1, 1);

	// qc_funcs.h: every function
	cb_puts (&hdr, "// Generated by FTEQCC -cppout (Tools/fteqcc/qcc_cpp.c) from Quake VR's QuakeC: do not edit.\n#pragma once\n"
				   "namespace qc\n{\n");
	for (i = 1; i < numfunctions; i++)
		if (cg_fname[i])
			cb_printf (&hdr, "void %s ();\n", cg_fname[i]);
	cb_puts (&hdr, "} // namespace qc\n");
	cg_writefile ("qc_funcs.h", &hdr);

	// one .cpp per source file, its functions in statement order
	for (i = 0; i <= nq; i++)
	{
		const char *fileof = i < nq ? (functions[order[i]].filen ? functions[order[i]].filen : "unknown") : NULL;
		if (curfile && (!fileof || strcmp (fileof, curfile)))
		{
			cgbuf_t whole = {0};
			char base[256];
			int k;
			cb_puts (&whole, "// Generated by FTEQCC -cppout (Tools/fteqcc/qcc_cpp.c) from ");
			cb_puts (&whole, curfile);
			cb_puts (&whole, ": do not edit.\n#include \"qccpp_rt.h\"\n#include \"qc_funcs.h\"\n\nnamespace\n{\nnamespace g\n{\n");
			for (k = 0; k < cg_gnames.n; k++)
				cb_printf (&whole, "constexpr int %s = %u;\n", cg_gnames.e[k].name, cg_gnames.e[k].ofs);
			cb_puts (&whole, "} // namespace g\nnamespace fld\n{\n");
			for (k = 0; k < cg_fldnames.n; k++)
				cb_printf (&whole, "constexpr int %s = %u;\n", cg_fldnames.e[k].name, cg_fldnames.e[k].ofs);
			cb_puts (&whole, "} // namespace fld\n} // namespace\n\nnamespace qc\n{\n");
			cb_putn (&whole, file.p, file.n);
			cb_puts (&whole, "} // namespace qc\n");
			cg_sanitize (curfile, base, sizeof (base));
			// a file name may repeat (a second run of functions from the same file): number it
			snprintf (outname, sizeof (outname), "qc_%03d_%s.cpp", nfiles++, base);
			cg_writefile (outname, &whole);
			free (whole.p);
			file.n = 0;
			cg_nameset_clear (&cg_gnames);
			cg_nameset_clear (&cg_fldnames);
		}
		if (!fileof)
			break;
		curfile = fileof;
		cg_function (&file, order[i], df[order[i]].first_statement, ends[order[i]], label);
	}

	// qc_table.cpp: the function table (progs index -> C++ function and name), the fields, the progs hash
	for (i = 0; i < numstatements; i++)
		hash = cg_fnv (hash, &st[i], sizeof (st[i]));
	for (i = 0; i < numfunctions; i++)
		hash = cg_fnv (hash, &df[i].first_statement, sizeof (int));
	cb_puts (&tab, "// Generated by FTEQCC -cppout (Tools/fteqcc/qcc_cpp.c): do not edit.\n#include \"qccpp_rt.h\"\n"
				   "#include \"qc_funcs.h\"\n\n");
	cb_printf (&tab, "extern const QCppProgs qcpp_progs;\nstatic const QCppFunction qcpp_functions[%d] = {\n", numfunctions);
	for (i = 0; i < numfunctions; i++)
	{
		if (cg_fname[i])
			cb_printf (&tab, "\t{\"%s\", &qc::%s},\n", strings + df[i].s_name, cg_fname[i]);
		else
			cb_printf (&tab, "\t{\"%s\", nullptr},\n", i ? strings + df[i].s_name : "");
	}
	cb_puts (&tab, "};\n");
	cb_printf (&tab, "static const QCppField qcpp_fields[%d] = {\n", numfielddefs ? numfielddefs : 1);
	for (i = 0; i < numfielddefs; i++)
		cb_printf (&tab, "\t{\"%s\", %u, %u},\n", strings + fields[i].s_name, fields[i].ofs, fields[i].type);
	if (!numfielddefs)
		cb_puts (&tab, "\t{\"\", 0, 0},\n");
	cb_puts (&tab, "};\n");
	cb_printf (&tab, "const QCppProgs qcpp_progs = {0x%08xu, %d, %d, %u, %d, qcpp_functions, qcpp_fields};\n", hash,
			   numstatements, numfunctions, numpr_globals, numfielddefs);
	cg_writefile ("qc_table.cpp", &tab);

	externs->Printf ("qcc_cpp: %d functions in %d files written to %s (hash %08x)%s\n", nq, nfiles, cg_dir, hash,
					 cg_errors ? " WITH ERRORS" : "");
	for (j = 0; j < numfunctions; j++)
		free (cg_fname[j]);
	free (cg_fname);
	free (order);
	free (ends);
	free (label);
	free (hdr.p);
	free (tab.p);
	free (file.p);
	free (funcs.p);
	cg_nameset_clear (&fnames);
}
