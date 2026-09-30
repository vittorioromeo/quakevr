// fteqcc's QC operator priorities, shown (docs/vr-port/CODE_STYLE.md, QuakeC; Misc/quakevr/check_qc_precedence.py).
// From this folder, with QC/build.sh's flags plus -Fwasm (qc.asm, the instructions), then `python dump.py [names]`:
//   fteqcc64 -O3 -Fautoproto -Olo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209
//            -Wno-F208 -Fwasm
// Add -Fcpriority for C's priorities. What fteqcc 64 builds is noted on each (QC priorities).
entity self;
float time;
void() main = {};
float arr[9];
float chain, last;
.float fld;

// The chainsaw (vr_chainsaw.qc): STORE_F chain -> cutting; the && is computed into a temporary and dropped.
void() t_and_assign = { float cutting; cutting = chain && time - last < 0.2; arr[0] = cutting; };
// The melee (vr_melee.qc): striker = (arr[i] == 3); the || is dropped. Not the array reads: see t_melee_paren.
void(float i) t_melee_old = { float striker; striker = arr[i] == 3 || arr[i] == 4; chain = striker; };
void(float i) t_melee_paren = { float striker; striker = (arr[i] == 3 || arr[i] == 4); chain = striker; };
// The same for ||, a field, +=, and !a && b: (x = a) || b, (self.fld = a) && b, (x += a) && b, (x = !a) && b.
void() t_assign_or = { float x; x = chain || last; arr[2] = x; };
void() t_field_and = { self.fld = chain && last; };
void() t_addeq_and = { float x = 1; x += chain && last; arr[4] = x; };
void() t_assign_not_and = { float x; x = !chain && last; arr[5] = x; };
// Fine: a declaration's initializer, a return, a call's argument, a comparison against arithmetic, a ternary.
void() t_decl_and = { float x = chain && last; arr[1] = x; };
float() t_and_cmp = { return chain && time - last < 0.2; };
void(float f) callee = { arr[0] = f; };
void() t_arg_and = { callee(chain && last); };
void() t_tern = { float x; x = chain ? 3 : 4; arr[3] = x; };
// ?: binds tighter than && and ||: a && (b ? 3 : 4), in a return, an initializer, an argument alike.
float() t_ret_and_tern = { return chain && last ? 3 : 4; };
void() t_decl_or_tern = { float x = chain || last ? 3 : 4; arr[8] = x; };
// && and || are one priority, left to right: (a || b) && c.
float(float a, float b, float c) t_or_and = { return a || b && c; };
// ! over a comparison: !(a == b) (warning F304). & before == and -: (a & b) == b, a - (a & b).
float(float a, float b) t_not_eq = { return !a == b; };
float(float a, float b) t_amp_eq = { return a & b == b; };
float(float a, float b) t_minus_amp = { return a - a & b; };
