  Con_Printf("has handtouch\n");


        Con_Printf("c %d>%d %d>%d %d>%d %d<%d %d<%d %d<%d\n", handposmin[0],
            target->v.absmax[0], handposmin[1], target->v.absmax[1],
            handposmin[2], target->v.absmax[2], handposmax[0],
            target->v.absmin[0], handposmax[1], target->v.absmin[1],
            handposmax[2], target->v.absmin[2]);



// TODO VR: this kinda works in E1M1 if put inside FlyMove
// TODO VR: test, what is this one for? How does it differ from world.cpp?

    auto doHandTouch = [&](vec3_t handpos, vec3_t handrot, int type) {
        vec3_t fwd, right, up, end;
        AngleVectors(handrot, fwd, right, up);
        fwd[0] *= 1.f;
        fwd[1] *= 1.f;
        fwd[2] *= 1.f;

        VectorCopy(handpos, end);
        VectorAdd(end, fwd, end);

        vec3_t mins{-2, -2, -2};
        vec3_t maxs{2, 2, 2};
        // trace_t trace = SV_Move(handpos, mins, maxs, end, type, ent);
        trace_t trace =
            SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, type, ent);

        // vec3_t impact;
        // trace_t trace = TraceLine(cl.handpos[1], end, impact);
        // SV_ClipMoveToEntity(sv.edicts, start, mins, maxs, end);

        /* if(trace.allsolid)
         { // entity is trapped in another solid
             // Con_Printf("all solid\n");
             return;
         }


         if(trace.fraction == 1)
         { // moved the entire distance
             // Con_Printf("entire dist\n");
             return;
         }*/

        if(!trace.ent)
        {
            // Con_Printf("no ent\n");
            return;
        }

        vec3_t aMin, aMax, bMin, bMax;
        VectorAdd(trace.ent->v.origin, trace.ent->v.mins, aMin);
        VectorAdd(trace.ent->v.origin, trace.ent->v.maxs, aMax);
        VectorAdd(handpos, mins, bMin);
        VectorAdd(handpos, maxs, bMax);

        if(quake::util::boxIntersection(aMin, aMax, bMin, bMax))
        {
            SV_Impact(ent, trace.ent, &entvars_t::handtouch);
        }
    };

    doHandTouch(ent->v.handpos, ent->v.handrot, MOVE_NORMAL);
    doHandTouch(ent->v.offhandpos, ent->v.offhandrot, MOVE_NORMAL);




void VR_Move(usercmd_t* cmd)
{
    if(!vr_enabled.value)
    {
        return;
    }

    // TODO VR: repetition of ofs calculation
    // TODO VR: adj unused? could be used to find position of muzzle
    //
    /*
    vec3_t adj;
    _VectorCopy(cl.handpos[1], adj);

    vec3_t ofs = {vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 1].value,
        vr_weapon_offset[weaponCVarEntry * VARS_PER_WEAPON + 2].value +
            vr_gunmodely.value};

    vec3_t fwd2, right, up;
    AngleVectors(cl.handrot[1], fwd2, right, up);
    fwd2[0] *= vr_gunmodelscale.value * ofs[2];
    fwd2[1] *= vr_gunmodelscale.value * ofs[2];
    fwd2[2] *= vr_gunmodelscale.value * ofs[2];
    VectorAdd(adj, fwd2, adj);
    */

    // TODO VR: not needed anymore, changing QC - what to do?
    //
    // vec3_t adjhandpos;
    // VectorCopy(cl.handpos[1], adjhandpos);
    // adjhandpos[2] -= vr_projectilespawn_z_offset.value;



class ParticleBuffer
{
private:
    // TODO VR: use raw buffer for more speed
    std::vector<particle_t> _particles;
    std::size_t _maxParticles;

public:
    void initialize(std::size_t maxParticles)
    {
        _maxParticles = maxParticles;
        _particles.reserve(maxParticles);
    }

    void cleanup()
    {
        _particles.erase(
            std::remove_if(_particles.begin(), _particles.end(),
                [](const particle_t& p) { return cl.time >= p.die; }),
            _particles.end());
    }

    [[nodiscard]] particle_t& create()
    {
        return _particles.emplace_back();
    }

    template <typename F>
    void forActive(F&& f)
    {
        for(auto& p : _particles)
        {
            f(p);
        }
    }

    [[nodiscard]] bool reachedMax() const noexcept
    {
        return _particles.size() == _maxParticles;
    }

    void clear()
    {
        _particles.clear();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return _particles.empty();
    }
};



class ParticleBuffer
{
private:
    particle_t* _particles;
    particle_t* _aliveEnd;
    particle_t* _end;

public:
    void initialize(const std::size_t maxParticles) noexcept
    {
        _particles = (particle_t*)Hunk_AllocName(
            maxParticles * sizeof(particle_t), "particles");
        _aliveEnd = _particles;
        _end = _particles + maxParticles;
    }

    void cleanup() noexcept
    {
        const auto it = std::remove_if(_particles, _aliveEnd,
            [](const particle_t& p) { return cl.time >= p.die; });

#if 0
        for(auto p = it; p != _aliveEnd; ++p)
        {
            p->~particle_t();
        }
#endif

        _aliveEnd = it;
    }

    [[nodiscard]] particle_t& create() noexcept
    {
#if 0
        const auto p = _aliveEnd++;
        new(p) particle_t;
        return *p;
#else
        return *_aliveEnd++;
#endif
    }

    template <typename F>
    void forActive(F&& f) noexcept
    {
        for(auto p = _particles; p != _aliveEnd; ++p)
        {
            f(*p);
        }
    }

    [[nodiscard]] bool reachedMax() const noexcept
    {
        return _aliveEnd == _end;
    }

    void clear() noexcept
    {
#if 0
        for(auto p = _particles; p != _aliveEnd; ++p)
        {
            p->~particle_t();
        }
#endif

        _aliveEnd = _particles;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return _aliveEnd == _particles;
    }
};


	// s = ftos(xdmg);
	// sprint (self, "OFFHANDDMG: ");
	// sprint (self, s);
	// sprint (self, "\n");


static void vec3lerp(vec3_t out, vec3_t start, vec3_t end, double f)
{
    out[0] = lerp(start[0], end[0], f);
    out[1] = lerp(start[1], end[1], f);
    out[2] = lerp(start[2], end[2], f);
}




// non-working smoothed hand rotation
#if 0
                if (i == 0) continue;

                const auto ox = cl.handrot[i][PITCH];
                const auto oy = cl.handrot[i][YAW];
                const auto oz = cl.handrot[i][ROLL];

                const auto tx = handrottemp[PITCH];
                const auto ty = handrottemp[YAW];
                const auto tz = handrottemp[ROLL];

                const glm::vec3 orig{ ox, oy, oz };

                // glm::fquat q{glm::radians(orig)};
                // q = glm::mix(glm::normalize(q), glm::normalize(glm::fquat(glm::radians(glm::vec3(tx, ty, tz)))), 0.05f);

                glm::mat3 m(toVec3(mat[0]), toVec3(mat[1]), toVec3(mat[2]));
                glm::fquat q(m);

                const glm::vec3 res{ glm::degrees(glm::eulerAngles(glm::normalize(q))) };

                const auto nx = res[PITCH];
                const auto ny = res[YAW];
                const auto nz = res[ROLL];

                auto fx = nx;
                auto fy = ny;
                auto fz = nz;

                if (oy > 90.f)
                {
                    fx -= 180.f;
                    fy -= 180.f;
                    fy *= -1.f;
                    fz += 180.f;

                    if (ox > 0.f)
                    {
                        fx += 360.f;
                    }
                }

                if (false)
                {
                    Con_Printf("%d %d %d | %d %d %d | %d %d %d\n",
                        (int)ox, (int)oy, (int)oz,
                        (int)nx, (int)ny, (int)nz,
                        (int)fx, (int)fy, (int)fz
                    );

                    quake::util::debugPrintSeparated(" ", (int)ox, (int)oy, (int)oz);
                    quake::util::debugPrint(" | ");
                    quake::util::debugPrintSeparated(" ", (int)nx, (int)ny, (int)nz);
                    quake::util::debugPrint(" | ");
                    quake::util::debugPrintSeparated(" ", (int)fx, (int)fy, (int)fz);
                    quake::util::debugPrint("\n");
                }

                handrottemp[0] = fx;
                handrottemp[1] = fy;
                handrottemp[2] = fz;
#endif
