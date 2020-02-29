
                // TODO VR:
                /*
                auto rads = glm::radians(glm::vec3(handrottemp[0],
                handrottemp[1], handrottemp[2])); auto quat = glm::fquat(rads);

                // glm::fquat qa =
                glm::fquat(glm::radians(glm::vec3(cl.handrot[i][0],
                cl.handrot[i][1], cl.handrot[i][2])));
                // glm::fquat qb =
                glm::fquat(glm::radians(glm::vec3(handrottemp[0],
                handrottemp[1], handrottemp[2])));
                // auto qc = glm::mix(qa, qb, 0.02f);
                float t0, t1, t2;
                glm::extractEulerAngleXYZ(glm::toMat4(quat), t2, t1, t0);

                auto qf = glm::degrees(glm::vec3(t0,t1,t2));

                char msgbuf[100];
                sprintf(msgbuf, "%f , %f , %f\n", rads[0], rads[1], rads[2]);
                OutputDebugStringA(msgbuf);
                */

                // glm::fquat a, b;
                //
                // {
                // 	vec3_t forward, right, up;
                // 	AngleVectors(handrottemp, forward, right, up);
                // 	a = glm::conjugate(glm::quatLookAt(toVec3(forward),
                // toVec3(up))); 	a = glm::normalize(a);
                // }
                //
                // {
                // 	vec3_t forward, right, up;
                // 	AngleVectors(cl.handrot[i], forward, right, up);
                // 	b = glm::quatLookAt(toVec3(forward), toVec3(up));
                // 	b = glm::normalize(b);
                // }


                // glm::fquat a(glm::vec3(cl.handrot[i][PITCH],
                // cl.handrot[i][YAW], cl.handrot[i][ROLL])); glm::fquat
                // b(glm::vec3(handrottemp[PITCH], handrottemp[YAW],
                // handrottemp[ROLL])); auto c = glm::mix(b, a, 0.1f); auto ro =
                // controllers[i].raworientation; auto ca =
                // glm::degrees(glm::eulerAngles(glm::conjugate(glm::quat(ro.w,ro.x,ro.y,ro.z))));
                // m = glm::quatLookAt(toVec3(forward), toVec3(up));
                // m = glm::rotate(m, vr_sbar_offset_pitch.value, glm::vec3(1,
                // 0, 0)); m = glm::rotate(m, vr_sbar_offset_yaw.value,
                // glm::vec3(0, 1, 0)); m = glm::rotate(m,
                // vr_sbar_offset_roll.value, glm::vec3(0, 0, 1)); m =
                // glm::normalize(m);

                //
                // cl.handrot[i][0] = ca.x;
                // cl.handrot[i][1] = ca.y;
                // cl.handrot[i][2] = ca.z;

                // static int counter = 0;
                // const int idx = (++counter / 700) % 12;
                // quake::util::debugPrint(idx);

if(i == 1){
    int ox = handrottemp[PITCH];
    int oy = handrottemp[YAW];
    int oz = handrottemp[ROLL];


                const glm::fquat q{glm::radians(glm::vec3{handrottemp[PITCH], handrottemp[YAW], handrottemp[ROLL]})};
                // const glm::vec3 res{glm::degrees(quaternion2Euler(glm::normalize(q), (RotSeq)idx))};
                const glm::vec3 res{ glm::degrees(glm::eulerAngles(glm::normalize(q))) };
                int nx = res[PITCH], ny = res[YAW], nz = res[ROLL];
                if (((int)res[PITCH] != (int)handrottemp[PITCH])
                    || ((int)res[YAW] != (int)handrottemp[YAW])
                    || ((int)res[ROLL] != (int)handrottemp[ROLL]))
                {

                    quake::util::debugPrintSeparated("\t", ox, oy, oz, '\n');
                    quake::util::debugPrintSeparated("\t", nx, ny, nz, "\n");
                    quake::util::debugPrintSeparated("\t", ox-nx, oy-ny, oz-nz, "\n\n");
                }



               //  auto fix = [](auto a) { return a < -90 ? a + 180  : a; };
                handrottemp[0] = nx;  [&] {
                    if (ox <= 0 && nx > 0) return nx - 180;
                    if (ox > 0 && nx <= 0) return nx + 180;
                    return nx;
                }();
                handrottemp[1] = ny;// fix(res.y);
                handrottemp[2] = nz; // fix(res.z);

                Con_Printf("%d %d %d | %d %d %d | %d %d %d\n",
                    (int)ox, (int)oy, (int)oz,
                    (int)res[PITCH], (int)res[YAW], (int)res[ROLL]
                );

                /*
                Con_Printf("%d %d %d | %d %d %d | %d %d %d\n",
                    (int)ox, (int)oy, (int)oz,
                    (int)res[PITCH], (int)res[YAW], (int)res[ROLL],
                    (int)handrottemp[0], (int)handrottemp[1], (int)handrottemp[2]
                );*/

                // quake::util::debugPrintSeparated(" ", handrottemp[PITCH], handrottemp[YAW], handrottemp[ROLL], '\n', '\n');
}
                VectorCopy(handrottemp, cl.handrot[i]);

                // vec3lerp(cl.handrot[i], cl.handrot[i], handrottemp, 0.02f);
            }

            if(cl.viewent.model)
            {
                aliashdr_t* hdr = (aliashdr_t*)Mod_Extradata(cl.viewent.model);
                Mod_Weapon(cl.viewent.model->name, hdr);
            }

            SetHandPos(0, player);
            SetHandPos(1, player);

            // TODO VR: interpolate based on weapon weight?
            VectorCopy(cl.handrot[1], cl.aimangles); // Sets the shooting angle
            // TODO: what sets the shooting origin?

            break;



            enum RotSeq { zyx, zyz, zxy, zxz, yxz,
              yxy, yzx, yzy, xyz, xyx,
              xzy, xzx };

static constexpr std::array<std::array<int, 3>, 12> mappings{
    std::array<int, 3>{2,1,0},
    std::array<int, 3>{2,1,2},
    std::array<int, 3>{2,0,1},
    std::array<int, 3>{2,0,2},
    std::array<int, 3>{1,0,2},

    std::array<int, 3>{1,0,1},
    std::array<int, 3>{1,2,0},
    std::array<int, 3>{1,2,1},
    std::array<int, 3>{0,1,2},
    std::array<int, 3>{0,1,0},

    std::array<int, 3>{0,2,1},
    std::array<int, 3>{0,2,0}
};

void twoaxisrot(double r11, double r12, double r21, double r31, double r32, double res[]) {
    res[0] = atan2(r11, r12);
    res[1] = acos(r21);
    res[2] = atan2(r31, r32);
}

void threeaxisrot(double r11, double r12, double r21, double r31, double r32, double res[]) {
    res[0] = atan2(r31, r32);
    res[1] = asin(r21);
    res[2] = atan2(r11, r12);
}

void quaternion2Euler(const glm::fquat& q, double res[], RotSeq rotSeq)
{
    switch (rotSeq) {
    case zyx:
        threeaxisrot(2 * (q.x * q.y + q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            -2 * (q.x * q.z - q.w * q.y),
            2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            res);
        break;

    case zyz:
        twoaxisrot(2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.z + q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.z - q.w * q.y),
            res);
        break;

    case zxy:
        threeaxisrot(-2 * (q.x * q.y - q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.z - q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            res);
        break;

    case zxz:
        twoaxisrot(2 * (q.x * q.z + q.w * q.y),
            -2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.x * q.z - q.w * q.y),
            2 * (q.y * q.z + q.w * q.x),
            res);
        break;

    case yxz:
        threeaxisrot(2 * (q.x * q.z + q.w * q.y),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            -2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.y + q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            res);
        break;

    case yxy:
        twoaxisrot(2 * (q.x * q.y - q.w * q.z),
            2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.x * q.y + q.w * q.z),
            -2 * (q.y * q.z - q.w * q.x),
            res);
        break;

    case yzx:
        threeaxisrot(-2 * (q.x * q.z - q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.y + q.w * q.z),
            -2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            res);
        break;

    case yzy:
        twoaxisrot(2 * (q.y * q.z + q.w * q.x),
            -2 * (q.x * q.y - q.w * q.z),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            2 * (q.y * q.z - q.w * q.x),
            2 * (q.x * q.y + q.w * q.z),
            res);
        break;

    case xyz:
        threeaxisrot(-2 * (q.y * q.z - q.w * q.x),
            q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z,
            2 * (q.x * q.z + q.w * q.y),
            -2 * (q.x * q.y - q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            res);
        break;

    case xyx:
        twoaxisrot(2 * (q.x * q.y + q.w * q.z),
            -2 * (q.x * q.z - q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.y - q.w * q.z),
            2 * (q.x * q.z + q.w * q.y),
            res);
        break;

    case xzy:
        threeaxisrot(2 * (q.y * q.z + q.w * q.x),
            q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z,
            -2 * (q.x * q.y - q.w * q.z),
            2 * (q.x * q.z + q.w * q.y),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            res);
        break;

    case xzx:
        twoaxisrot(2 * (q.x * q.z - q.w * q.y),
            2 * (q.x * q.y + q.w * q.z),
            q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z,
            2 * (q.x * q.z + q.w * q.y),
            -2 * (q.x * q.y - q.w * q.z),
            res);
        break;
    default:
        assert(false);
        break;
    }
}

glm::vec3 quaternion2Euler(const glm::fquat& q, RotSeq rs)
{
    // zyx {2,1,0},

    const auto& map = mappings[(int)rs];
    double buf[3];
    quaternion2Euler(q, buf, rs);
    glm::vec3 res;
    res[map[0]] = buf[0];
    res[map[1]] = buf[1];
    res[map[2]] = buf[2];
    return res;
}