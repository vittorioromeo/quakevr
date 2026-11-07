// ea_fields: the QC field table and how much of each live edict is used (THROWAWAY)
static unsigned char eaUsed[8192];
static int eaNz[8192];
struct EaPfx { char p[16]; int slots; int n; };
static EaPfx eaPf[64];
static void ea_fields_f()
{
    qcvm_t* vm = &sv.qcvm;
    const int nf = vm->progs->numfielddefs, words = vm->progs->entityfields;
    memset(eaUsed, 0, sizeof eaUsed); memset(eaNz, 0, sizeof eaNz); memset(eaPf, 0, sizeof eaPf);
    int npf = 0;
    int nameCount = 0, floatN = 0, vecN = 0, otherN = 0;
    for(int i = 0; i < nf; i++)
    {
        const ddef_t* d = &vm->fielddefs[i];
        const int ty = d->type & ~DEF_SAVEGLOBAL;
        const char* nm = vm->strings + d->s_name;
        const int sz = ty == ev_vector ? 3 : 1;
        const size_t L = strlen(nm);
        if(ty == ev_float && L > 2 && nm[L - 2] == '_' && (nm[L - 1] == 'x' || nm[L - 1] == 'y' || nm[L - 1] == 'z')) continue;
        nameCount++;
        if(ty == ev_vector) vecN++; else if(ty == ev_float) floatN++; else otherN++;
        for(int k = 0; k < sz && (int)d->ofs + k < 8192; k++) eaUsed[d->ofs + k] = 1;
        char p[16]; int c = 0;
        while(nm[c] && nm[c] != '_' && c < 14) { p[c] = nm[c]; c++; }
        p[c] = 0; if(nm[c] != '_') strcpy(p, "(none)");
        int j = 0; for(; j < npf; j++) if(!strcmp(eaPf[j].p, p)) break;
        if(j == npf && npf < 64) { strcpy(eaPf[npf].p, p); npf++; }
        if(j < 64) { eaPf[j].slots += sz; eaPf[j].n++; }
    }
    int usedSlots = 0; for(int i = 0; i < words; i++) usedSlots += eaUsed[i];
    Con_Printf("EF fielddefs %d named %d (float %d vector %d other %d) words %d usedslots %d\n", nf, nameCount, floatN, vecN, otherN, words, usedSlots);
    for(int r = 0; r < 12; r++)
    {
        int b = -1; for(int j = 0; j < npf; j++) if(eaPf[j].slots > 0 && (b < 0 || eaPf[j].slots > eaPf[b].slots)) b = j;
        if(b < 0) break;
        Con_Printf("EF prefix %-10s fields %4d slots %4d\n", eaPf[b].p, eaPf[b].n, eaPf[b].slots); eaPf[b].slots = -eaPf[b].slots;
    }
    int live = 0, free_ = 0, nzMax = 0; long long nzTotal = 0, linesTotal = 0;
    for(int e = 0; e < vm->num_edicts; e++)
    {
        edict_t* ed = (edict_t*)((byte*)vm->edicts + (size_t)e * vm->edict_size);
        if(ed->free) { free_++; continue; }
        live++;
        const int* w = (const int*)&ed->v; int cnt = 0; unsigned char line[512] = {0};
        for(int i = 0; i < words; i++) if(w[i]) { cnt++; if(i < 8192) eaNz[i]++; line[(252 + i * 4) / 64] = 1; }
        nzTotal += cnt; if(cnt > nzMax) nzMax = cnt;
        for(int l = 0; l < 512; l++) linesTotal += line[l];
    }
    Con_Printf("EF edicts %d live %d free %d: avg nonzero %.1f max %d of %d; 64B-lines-with-data %.1f of %d\n",
        vm->num_edicts, live, free_, live ? (double)nzTotal / live : 0.0, nzMax, words, live ? (double)linesTotal / live : 0.0, (252 + words * 4 + 63) / 64);
    int b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0;
    for(int i = 0; i < words && live; i++)
    {
        double f = (double)eaNz[i] / live;
        if(eaNz[i] == 0) b0++; else if(f < 0.005) b1++; else if(f < 0.05) b2++; else if(f < 0.5) b3++; else b4++;
    }
    Con_Printf("EF slots nonzero in: no live edict %d, <0.5pct %d, 0.5-5pct %d, 5-50pct %d, >50pct %d\n", b0, b1, b2, b3, b4);
    // offsets of more hot fields (word offset * 4 + 252)
    PR_SwitchQCVM(&sv.qcvm);
    int offs[16]; const char* nms[16] = {"takedamage", "nextthink", "think", "classname", "owner", "velocity", "enemy", "touch", "model", "modelindex", "effects", "frame", "ltime", "groundentity", "targetname", "target"};
    for(int i = 0; i < 16; i++) { offs[i] = -1; for(int k = 0; k < nf; k++) if(!strcmp(vm->strings + vm->fielddefs[k].s_name, nms[i])) offs[i] = (int)vm->fielddefs[k].ofs * 4 + 252; }
    PR_SwitchQCVM(NULL);
    Con_Printf("EF off: %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d\n", nms[0], offs[0], nms[1], offs[1], nms[2], offs[2], nms[3], offs[3], nms[4], offs[4], nms[5], offs[5], nms[6], offs[6], nms[7], offs[7]);
    Con_Printf("EF off: %s %d %s %d %s %d %s %d %s %d %s %d %s %d %s %d\n", nms[8], offs[8], nms[9], offs[9], nms[10], offs[10], nms[11], offs[11], nms[12], offs[12], nms[13], offs[13], nms[14], offs[14], nms[15], offs[15]);
    Con_Printf("EF_DONE\n");
    {
        int mt[16] = {0}, idle = 0, solidNotN = 0, withModel = 0, monsters = 0;
        for(int e = 0; e < vm->num_edicts; e++)
        {
            edict_t* ed = (edict_t*)((byte*)vm->edicts + (size_t)e * vm->edict_size);
            if(ed->free) continue;
            mt[(int)ed->v.movetype & 15]++;
            if((int)ed->v.movetype == MOVETYPE_NONE && !(ed->v.nextthink > 0.f)) idle++;
            if((int)ed->v.solid == SOLID_NOT) solidNotN++;
            if(ed->v.modelindex) withModel++;
            if((int)ed->v.flags & FL_MONSTER) monsters++;
        }
        volatile float sink = 0; const int reps = 100;
        const long long t0 = EA_Now();
        for(int r = 0; r < reps; r++)
            for(int e = 0; e < vm->num_edicts; e++)
            {
                edict_t* ed = (edict_t*)((byte*)vm->edicts + (size_t)e * vm->edict_size);
                if(ed->free) continue;
                sink = sink + ed->v.movetype + ed->v.nextthink;
            }
        const long long t1 = EA_Now(); QueryPerformanceFrequency((LARGE_INTEGER*)&eaFreq);
        Con_Printf("EF movetype none %d push %d noclip %d step %d walk %d toss %d bounce %d fly %d flymissile %d gib %d; idle(none, no think) %d; solid_not %d; with model %d; monsters %d\n",
            mt[MOVETYPE_NONE], mt[MOVETYPE_PUSH], mt[MOVETYPE_NOCLIP], mt[MOVETYPE_STEP], mt[MOVETYPE_WALK], mt[MOVETYPE_TOSS], mt[MOVETYPE_BOUNCE], mt[MOVETYPE_FLY], mt[MOVETYPE_FLYMISSILE], mt[MOVETYPE_GIB], idle, solidNotN, withModel, monsters);
        Con_Printf("EF skeleton walk (free, movetype, nextthink) %.2f us per pass of %d edicts = %.1f ns/edict\n", 1e6 * (t1 - t0) / eaFreq / reps, vm->num_edicts, 1e9 * (t1 - t0) / eaFreq / reps / vm->num_edicts);
    }
}
