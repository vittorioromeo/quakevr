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













 case VrAimMode::e_CONTROLLER:
        {
            cl.viewangles[PITCH] = orientation[PITCH];
            cl.viewangles[YAW] = orientation[YAW];

            vec3_t mat[3];
            vec3_t matTmp[3];

            vec3_t rotOfs = {vr_gunangle.value, vr_gunyaw.value, 0};

            vec3_t gunMatPitch[3];
            CreateRotMat(0, rotOfs[0], gunMatPitch); // pitch

            vec3_t gunMatYaw[3];
            CreateRotMat(1, rotOfs[1], gunMatYaw); // yaw

            vec3_t gunMatRoll[3];
            CreateRotMat(2, rotOfs[2], gunMatRoll); // roll

            for(int i = 0; i < 2; i++)
            {
                RotMatFromAngleVector(controllers[i].orientation, mat);

                R_ConcatRotations(gunMatRoll, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatPitch, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                R_ConcatRotations(gunMatYaw, mat, matTmp);
                for(int j = 0; j < 3; ++j)
                {
                    VectorCopy(matTmp[j], mat[j]);
                }

                vec3_t handrottemp;
                AngleVectorFromRotMat(mat, handrottemp);
                VectorCopy(handrottemp, cl.handrot[i]);
            }

            if(cl.viewent.model)
            {
                auto* hdr = (aliashdr_t*)Mod_Extradata(cl.viewent.model);
                Mod_Weapon(cl.viewent.model->name, hdr);

                // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
                // testhdr->flags |= EF_GRENADE;
                // VectorScale(testhdr->scale_origin, 0.5f,
                // testhdr->scale_origin);

                // BModels cannot be scaled, doesnt work
                // qmodel_t* test = Mod_ForName("maps/b_shell1.bsp", true);
                // auto* testhdr = (aliashdr_t*)Mod_Extradata(test);
                // VectorScale(testhdr->scale_origin, 0.5f,
                // testhdr->scale_origin);
            }

            if(cl.offhand_viewent.model)
            {
                // aliashdr_t* hdr =
                // (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model);
                // Mod_Weapon(cl.offhand_viewent.model->name, hdr);

                ApplyMod_Weapon(VR_GetOffHandFistCvarEntry(),
                    (aliashdr_t*)Mod_Extradata(cl.offhand_viewent.model));
            }

            SetHandPos(0, player);
            SetHandPos(1, player);

            Con_Printf("%d %d %d | ", (int)cl.handpos[1][0],
                (int)cl.handpos[1][1], (int)cl.handpos[1][2]);

            Con_Printf("%d %d %d\n", (int)cl.handrot[1][0],
                (int)cl.handrot[1][1], (int)cl.handrot[1][2]);

            auto up = glm::vec3{0, 0, 1};

            auto m = glm::lookAtRH(toVec3(cl.handpos[1]), toVec3(cl.handpos[0]),
                up);

            vr::HmdMatrix34_t mm;
            for(int x = 0; x < 3; ++x)
            {
                for(int y = 0; y < 4; ++y)
                {
                    mm.m[x][y] = m[x][y];
                }
            }

            auto ypr = QuatToYawPitchRoll(Matrix34ToQuaternion(mm));

            const auto nx = ypr[PITCH];
            const auto ny = ypr[YAW];
            const auto nz = ypr[ROLL];

            auto fy = nx;
            auto fx = ny;
            auto fz = nz;

           if(fy > 90.f)
            {
                fx -= 180.f;
                fy -= 180.f;
                fy *= -1.f;
                fz += 180.f;

                if(fx > 0.f)
                {
                    fx += 360.f;
                }
            }


            cl.handrot[1][PITCH] = fx;
            cl.handrot[1][YAW] = fy;
            cl.handrot[1][ROLL] = fz;

            // TODO VR: interpolate based on weapon weight?
            VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
            // TODO VR: what sets the shooting origin?

            // TODO VR: teleportation stuff
            VR_DoTeleportation();
            break;
        }
    }
    cl.viewangles[ROLL] = orientation[ROLL];




// -----------------------------------------------------------------------
    // VR: Detect & resolve hand collisions against the world or entities.

    // Positions.
    auto currHandPos = toVec3(cl.handpos[index]);
    const auto desiredHandPos = toVec3(finalPre);

    // Size of hand hitboxes.
    vec3_t mins{-1.f, -1.f, -1.f};
    vec3_t maxs{1.f, 1.f, 1.f};

    // Start around the upper torso, not actual center of the player.
    vec3_t adjPlayerOrigin;
    VR_GetAdjustedPlayerOrigin(adjPlayerOrigin, player);

    // TODO VR:
    const float gunLength = 13;

    // 1. If hands get too far, bring them closer to the player.
    constexpr auto maxHandPlayerDiff = 100.f;
    const auto handPlayerDiff = currHandPos - toVec3(adjPlayerOrigin);
    if (glm::length(handPlayerDiff) > maxHandPlayerDiff)
    {
        const auto dir = glm::normalize(handPlayerDiff);
        currHandPos[0] = dir[0] * maxHandPlayerDiff;
        currHandPos[1] = dir[1] * maxHandPlayerDiff;
        currHandPos[2] = dir[2] * maxHandPlayerDiff;
    }

    // 2. Trace the current hand position against the desired one.
    // `SV_Move` detects entities as well, not just geometry.
    const trace_t handTrace =
        SV_Move(cl.handpos[index], mins, maxs, finalPre, MOVE_NORMAL, sv_player);





    vec3_t forward, right, up;
    AngleVectors(cl.handrot[1], forward, right, up);



    // Trace from upper torso to desired final location. `SV_Move` detects
    // entities as well, not just geometry.
    const trace_t trace =
        SV_Move(adjPlayerOrigin, mins, maxs, finalPre, MOVE_NORMAL, sv_player);

    vec3_t adjFinalPre;
    VectorCopy(finalPre, adjFinalPre);

    adjFinalPre[0] += forward[0] * gunLength;
    adjFinalPre[1] += forward[1] * gunLength;
    adjFinalPre[2] += forward[2] * gunLength;

    // TODO VR:
    const trace_t gunTrace =
        SV_Move(cl.handpos[1], mins, maxs, adjFinalPre, MOVE_NORMAL, sv_player);

    // Origin of the trace.
    const auto orig = quake::util::toVec3(adjPlayerOrigin);

    // Final position before collision resolution.
    const auto pre = quake::util::toVec3(finalPre);

    // Final position after full collision resolution.
    const auto crop = quake::util::toVec3(trace.endpos);

    // TODO VR:
    auto gunCrop = quake::util::toVec3(gunTrace.endpos) -=
        toVec3(forward) * gunLength;

    // Compute final collision resolution position, starting from the desired
    // position and resolving only against the collision plane's normal vector.
    VectorCopy(finalPre, finalVec);
    if(trace.fraction < 1.f)
    {
        VectorCopy(finalPre, finalVec);
        for(int i = 0; i < 3; ++i)
        {
            if(trace.plane.normal[i] != 0)
            {
                finalVec[i] = crop[i];
            }
        }
    }




    // Con_Printf("newup: %.2f, %.2f, %.2f\n", newUp[0], newUp[1], newUp[2]);
    // Con_Printf("mixup: %.2f, %.2f, %.2f\n\n", mixUp[0], mixUp[1], mixUp[2]);

    // const auto oldDir = getDirectionVectorFromPitchYawRoll(cl.aimangles);
    // const auto newDir = getDirectionVectorFromPitchYawRoll(cl.handrot[1]);
    // Con_Printf("dir: %.2f, %.2f, %.2f\n", newDir[0], newDir[1], newDir[2]);
    // const auto mixDir = glm::slerp(oldDir, newDir, 0.05f);

    // handpos
    // const auto pitch = glm::radians(cl.handrot[1][PITCH]);
    // const auto yaw = glm::radians(cl.handrot[1][YAW]);
    // const auto roll = glm::radians(cl.handrot[1][ROLL]);

    //  const glm::vec3 oldAngles{oldx, oldy, oldz};

    // auto dx = sin(yaw);
    // auto dy = -(sin(pitch)*cos(yaw));
    // auto dz = -(cos(pitch)*cos(yaw));

    //    auto dx = std::cos(yaw) * std::cos(pitch);
    //    auto dy = std::sin(yaw) * std::cos(pitch);
    //    auto dz = std::sin(pitch);

    // const auto [fwd, right, up] = getGlmAngledVectors(newDir);
    // Con_Printf("fwd: %.2f, %.2f, %.2f\n", fwd[0], fwd[1], fwd[2]);





    const auto mySlerp = [](auto start, auto end, float percent) {
        // Dot product - the cosine of the angle between 2 vectors.
        float dot = glm::dot(start, end);
        // Clamp it to be in the range of Acos()
        // This may be unnecessary, but floating point
        // precision can be a fickle mistress.
        dot = std::clamp(dot, -1.0f, 1.0f);
        // Acos(dot) returns the angle between start and end,
        // And multiplying that by percent returns the angle between
        // start and the final result.
        float theta = acos(dot) * percent;
        auto RelativeVec = glm::normalize(end - start * dot);
        // Orthonormal basis
        // The final result.
        return ((start * cos(theta)) + (RelativeVec * sin(theta)));
    };

    const auto [oldFwd, oldRight, oldUp] = getGlmAngledVectors(cl.prevhandrot[1]);
    const auto [newFwd, newRight, newUp] = getGlmAngledVectors(cl.handrot[1]);

    const auto nOldFwd = glm::normalize(oldFwd);
    const auto nOldUp = glm::normalize(oldUp);
    const auto nNewFwd = glm::normalize(newFwd);
    const auto nNewUp = glm::normalize(newUp);

    const float frametime = cl.time - cl.oldtime;
    const auto factor = 0.1f + ((vr_2h_aim_transition / 10.f) * 2.f);
    const auto ftw = (factor * frametime) * 100.f;

    const auto slerpFwd = glm::slerp(nOldFwd, nNewFwd, ftw);
    const auto slerpUp = glm::slerp(nOldUp, nNewUp, ftw);

    const auto anyNan = [](const glm::vec3& v) {
        return std::isnan(v[0]) || std::isnan(v[1]) || std::isnan(v[2]);
    };

    const auto mixFwd = anyNan(slerpFwd) ? nNewFwd : slerpFwd;
    const auto mixUp = anyNan(slerpUp) ? nNewUp : slerpUp;

    // TODO VR: should be mixUp, this causes issues  when meleeing
    const auto [p, y, r] = pitchYawRollFromDirectionVector(mixUp, mixFwd);
    // Con_Printf("pyr: %.2f, %.2f, %.2f\n", p, y, r);

    const auto fixRoll = cl.handrot[1][ROLL]+360.f;

    Con_Printf("%.2f, %.2f, %.2f\n", cl.prevhandrot[1][ROLL] + 360.f,
        LerpDegrees(cl.prevhandrot[1][ROLL] + 360.f, fixRoll, ftw), fixRoll);

    cl.handrot[1][PITCH] = p;
    cl.handrot[1][YAW] = y;
    cl.handrot[1][ROLL] = r;// LerpDegrees(cl.prevhandrot[1][ROLL] + 360.f, fixRoll, ftw) - 360.f;

    VectorCopy(cl.handrot[1], cl.prevhandrot[1]);

    // TODO VR: interpolate based on weapon weight?
    VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
    // TODO VR: what sets the shooting origin?

    // TODO VR: teleportation stuff
    VR_DoTeleportation();





float LerpDegrees(float a, float b,
    float lerpFactor) // Lerps from angle a to b (both between 0.f and 360.f),
                      // taking the shortest path
{
    float result;
    float diff = b - a;
    if(diff < -180.f)
    {
        // lerp upwards past 360
        b += 360.f;
        result = lerp(a, b, lerpFactor);
        if(result >= 360.f)
        {
            result -= 360.f;
        }
    }
    else if(diff > 180.f)
    {
        // lerp downwards past 0
        b -= 360.f;
        result = lerp(a, b, lerpFactor);
        if(result < 0.f)
        {
            result += 360.f;
        }
    }
    else
    {
        // straight lerp
        result = lerp(a, b, lerpFactor);
    }

    return result;
}

    template <typename... Ts>
    [[nodiscard]] constexpr auto makeAdjustedMenuLabels(const Ts&... labels)
    {
        constexpr auto maxLen = 26;

        assert(((strlen(labels) <= maxLen) && ...));
        return std::array{
            (std::string(maxLen - strlen(labels), ' ') + labels)...};
    }



    // TODO VR:
    if(false && !(clip & 1))
    {
        // floor not detected
        // Con_Printf("floor not detected\n");

        constexpr glm::vec3 zOff{0.f, 0.f, 1.f};

        const auto traceZLine = [&](const glm::vec3& point) {
            // TODO VR: find all these traces with two zeros and create function
            return SV_Move(point + zOff, vec3_zero, vec3_zero, point - zOff,
                MOVE_NORMAL, ent);
        };

        const auto doTrace = [&](const glm::vec3& point) {
            const trace_t t = traceZLine(point);

            if(!quake::util::hitSomething(t) || t.plane.normal[2] <= 0.7)
            {
                return false;
            }

            if(t.ent->v.solid == SOLID_BSP)
            {
                ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
                ent->v.groundentity = EDICT_TO_PROG(t.ent);
            }

            return true;
        };

        const auto diff = ent->v.absmax - ent->v.absmin;
        const glm::vec3 offX{diff[0], 0.f, 0.f};
        const glm::vec3 offY{0.f, diff[1], 0.f};
        const glm::vec3 offXY{diff[0], diff[1], 0.f};
        const glm::vec3 offXYHalf = offXY / 2.f;

        const bool anyGroundHit =             //
            doTrace(ent->v.absmin) ||         //
            doTrace(ent->v.absmin + offX) ||  //
            doTrace(ent->v.absmin + offY) ||  //
            doTrace(ent->v.absmin + offXY) || //
            doTrace(ent->v.absmin + offXYHalf);

        (void)anyGroundHit;
    }



    if(false)
    {
        cl.handvel[index] =
            (cl.handpos[index] - lastPlayerTranslation) - oldHandpos;
        const auto [vFwd, vRight, vUp] = getAngledVectors(cl.handrot[index]);

        cl.handvel[index] +=
            glm::cross(controllers[index].a_velocity, vUp * 0.1f);
    }

    if(false)
    {
        cl.handvel[index] =
            Vec3RotateZ(cl.handvel[index], vrYaw * M_PI_DIV_180);
    }
