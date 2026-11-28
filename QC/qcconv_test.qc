#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// The QuakeC <-> C boundary's costs (qcconv 2026-11-28; docs/vr-port/QC_VM.md, "The boundary"): the same counted
// loop empty, with a builtin call, with a QuakeC call and with an entity field load and store. Timed by
// "vr_qcexec_bench qcconv_bench_<kind> <calls> <n>": ns a call over n, less the empty loop's, is one of each.
// ----------------------------------------------------------------------------

float qcconv_bench_sink;

float(float x) qcconv_bench_id =
{
    return x;
};

float(float n) qcconv_bench_empty =
{
    local float i = 0, x = 1;
    while(i < n)
    {
        x = x + 1;
        i = i + 1;
    }
    qcconv_bench_sink = x;
    return x;
};

float(float n) qcconv_bench_builtin =
{
    local float i = 0, x = 1;
    while(i < n)
    {
        x = fabs(x) + 1;
        i = i + 1;
    }
    qcconv_bench_sink = x;
    return x;
};

float(float n) qcconv_bench_vbuiltin =
{
    local float i = 0, x = 1;
    local vector v = '1 2 3';
    while(i < n)
    {
        x = vlen(v) + 1;
        i = i + 1;
    }
    qcconv_bench_sink = x;
    return x;
};

float(float n) qcconv_bench_qccall =
{
    local float i = 0, x = 1;
    while(i < n)
    {
        x = qcconv_bench_id(x) + 1;
        i = i + 1;
    }
    qcconv_bench_sink = x;
    return x;
};

float(float n) qcconv_bench_field =
{
    local float i = 0, x = 1;
    while(i < n)
    {
        x = world.frags + 1;
        i = i + 1;
    }
    qcconv_bench_sink = x;
    return x;
};

// The combined instructions' check (vr_qcexec_fuse; QC_VM.md): code whose statements make the pairs of
// vr_qcexec.c's QX_FUSED (conditions with && || == != < > <= >=, entity compares, field loads and stores, string
// compares, calls with 1-3 arguments after their stores, vector stores). "vr_qcexec_bench qcconv_fuse_check 200 <n>"
// says whether every loop returns the same bits and runs the same statements.
entity qcconv_bench_ent;

float(float a, float b) qcconv_bench_add2 =
{
    return a + b;
};

float(float a, float b, float c) qcconv_bench_add3 =
{
    if(a > b && b > c)
        return a;
    return a + b + c;
};

float(float n) qcconv_fuse_check =
{
    local float i = 0, h = 7, x, y;
    local entity e;
    local vector v = '0 0 0';
    if(!qcconv_bench_ent)
        qcconv_bench_ent = spawn();
    e = qcconv_bench_ent;
    e.netname = "abc";
    while(i < n)
    {
        x = i * 0.5;
        y = n - i;
        e.frags = x;
        e.oldorigin = v;
        v = e.oldorigin + '1 0 0';
        if(x > 3 && y < 40)
            h = h + 1;
        if(x == 2 || y == 5 || x == y)
            h = h * 2;
        if(x != y && !(h > 1000))
            h = h + 3;
        if(x <= y)
            h = h - 1;
        if(x >= y || i == 3)
            h = h + 5;
        if(e.frags != x)
            h = h + 100;
        if(e.frags == x && e != world)
            h = h + 2;
        if(e.netname == "abc")
            h = h + 1;
        if(!e.frags)
            h = h + 9;
        if(e.frags < 4 || e.frags > 90)
            h = h + 4;
        if(e.frags <= 10)
            h = h + 6;
        h = qcconv_bench_add2(h, x);
        h = qcconv_bench_add3(h, y, x);
        h = fabs(h);
        if(h > 100000)
            h = h - 99999;
        i = i + 1;
    }
    return h + vlen(v);
};

// (referenced, so the compiler keeps their names for vr_qcexec_bench)
void() qcconv_bench_keep =
{
    local float(float) f;
    f = qcconv_bench_empty;
    f = qcconv_bench_builtin;
    f = qcconv_bench_vbuiltin;
    f = qcconv_bench_qccall;
    f = qcconv_bench_field;
    f = qcconv_fuse_check;
    qcconv_bench_sink = (f != world.think);
};
#endif
