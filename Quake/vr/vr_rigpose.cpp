// vr_rigpose.cpp -- a ragdoll rig's bones posed by the model's frames, and between them along a spline (vr_rigpose.hpp).

#include "vr_rigpose.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"


namespace qvr::rigpose
{

Rigid key(const ragdoll::Rig& rig, int pose, int b)
{
    Rigid r;
    ragdoll::bonePose(rig, pose, b, r.rot, r.pos);
    return r;
}

Spline spline(float t, const float span[3], float tension)
{
    Spline s;
    if(t <= 0.f)
    {
        return s;
    }
    if(t >= 1.f)
    {
        s.w[1] = 0.f;
        s.w[2] = 1.f;
        s.rot[1] = 1.f;
        return s;
    }
    const float d0 = span[0], d1 = span[1], d2 = span[2];
    const float k = (1.f - tension) * d1;
    // The tangents at keys 1 and 2 (times the segment's span): a(p1 - p0) + b(p2 - p1), c(p2 - p1) + e(p3 - p2).
    const float a = k * d1 / ((d0 + d1) * d0);
    const float b = k * d0 / ((d0 + d1) * d1);
    const float c = k * d2 / ((d1 + d2) * d1);
    const float e = k * d1 / ((d1 + d2) * d2);
    const float t2 = t * t, t3 = t2 * t;
    const float h00 = 2.f * t3 - 3.f * t2 + 1.f, h10 = t3 - 2.f * t2 + t, h01 = -2.f * t3 + 3.f * t2, h11 = t3 - t2;
    s.w[0] = -h10 * a;
    s.w[1] = h00 + h10 * (a - b) - h11 * c;
    s.w[2] = h01 + h10 * b + h11 * (c - e);
    s.w[3] = h11 * e;
    s.rot[0] = h10 * a;
    s.rot[1] = h01 + h10 * b + h11 * c;
    s.rot[2] = h11 * e;
    return s;
}

glm::vec3 toVector(const glm::quat& q)
{
    const glm::quat h = q.w < 0.f ? -q : q;
    const glm::vec3 v{h.x, h.y, h.z};
    const float s = za::sqrt(glm::dot(v, v));
    if(s < 1e-7f)
    {
        return v * 2.f;
    }
    return v * (2.f * za::atan2(s, h.w) / s);
}

glm::quat fromVector(const glm::vec3& v)
{
    const float angle = za::sqrt(glm::dot(v, v));
    if(angle < 1e-7f)
    {
        return glm::normalize(glm::quat{1.f, v.x * 0.5f, v.y * 0.5f, v.z * 0.5f});
    }
    const float s = za::sin(angle * 0.5f) / angle;
    return glm::quat{za::cos(angle * 0.5f), v.x * s, v.y * s, v.z * s};
}

glm::quat rotation(const glm::quat q[4], const Spline& s)
{
    glm::quat k[4] = {q[0], q[1], q[2], q[3]};
    for(int i = 1; i < 4; i++)
    {
        if(glm::dot(k[i], k[i - 1]) < 0.f)
        {
            k[i] = -k[i];
        }
    }
    const glm::vec3 turn = s.rot[0] * toVector(k[1] * glm::conjugate(k[0])) + s.rot[1] * toVector(k[2] * glm::conjugate(k[1])) +
                           s.rot[2] * toVector(k[3] * glm::conjugate(k[2]));
    return glm::normalize(fromVector(turn) * k[1]);
}

namespace
{
// along()'s work, with `keyOf(i, b)` bone b's key in the i-th of the four poses.
template <typename KeyOf>
void alongBy(const ragdoll::Rig& rig, const KeyOf& keyOf, const Spline& s, uint32_t alone, Rigid* out)
{
    for(int b = 0; b < rig.numBones; b++)
    {
        const ragdoll::Bone& bone = rig.bones[b];
        Rigid k[4];
        glm::quat q[4];
        for(int i = 0; i < 4; i++)
        {
            k[i] = keyOf(i, b);
            q[i] = k[i].rot;
        }
        const glm::quat rot = rotation(q, s);
        const glm::vec3 c = bone.pivot;
        const int p = bone.parent;
        const bool rooted = p >= 0 && p < b && bone.joint != ragdoll::Joint::Loose && !(alone & (1u << p));
        glm::vec3 at = rooted ? apply(out[p], c) : glm::vec3{0.f};
        for(int i = 0; i < 4; i++)
        {
            // (the joint's gap: where this bone's fit puts it less where its parent's does)
            at += s.w[i] * (rooted ? apply(k[i], c) - apply(keyOf(i, p), c) : apply(k[i], c));
        }
        out[b].rot = rot;
        out[b].pos = at - rot * c;
    }
}
} // namespace

void along(const ragdoll::Rig& rig, const int poses[4], const Spline& s, uint32_t alone, Rigid* out)
{
    along(rig, nullptr, poses, s, alone, out);
}

void along(const ragdoll::Rig& rig, const Rigid* keys, const int poses[4], const Spline& s, uint32_t alone, Rigid* out)
{
    alongBy(rig, [&](int i, int b) { return keys ? keys[poses[i] * rig.numBones + b] : key(rig, poses[i], b); }, s, alone, out);
}

void alongSlots(const ragdoll::Rig& rig, const Rigid (*keys)[ragdoll::maxBones], const Spline& s, uint32_t alone, Rigid* out)
{
    alongBy(rig, [&](int i, int b) { return keys[i][b]; }, s, alone, out);
}

Rigid fitRigid(const glm::vec3* a, const glm::vec3* b, int n)
{
    Rigid r;
    if(n <= 0)
    {
        return r;
    }
    glm::dvec3 ca{0.0}, cb{0.0};
    for(int i = 0; i < n; i++)
    {
        ca += glm::dvec3{a[i]};
        cb += glm::dvec3{b[i]};
    }
    ca /= static_cast<double>(n);
    cb /= static_cast<double>(n);
    double S[3][3] = {};
    for(int i = 0; i < n; i++)
    {
        const glm::dvec3 da = glm::dvec3{a[i]} - ca, db = glm::dvec3{b[i]} - cb;
        for(int x = 0; x < 3; x++)
        {
            for(int y = 0; y < 3; y++)
            {
                S[x][y] += da[x] * db[y];
            }
        }
    }
    double N[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2], S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0], S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2], S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1], -S[0][0] - S[1][1] + S[2][2]}};
    double V[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    for(int sweep = 0; sweep < 30; sweep++)
    {
        double off = 0.0;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += N[p][q] * N[p][q];
            }
        }
        if(off < 1e-20)
        {
            break;
        }
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(N[p][q]) < 1e-18)
                {
                    continue;
                }
                const double th = (N[q][q] - N[p][p]) / (2.0 * N[p][q]);
                const double t = (th >= 0.0 ? 1.0 : -1.0) / (za::abs(th) + za::sqrt(th * th + 1.0));
                const double c = 1.0 / za::sqrt(t * t + 1.0), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const double kp = N[k][p], kq = N[k][q];
                    N[k][p] = c * kp - s * kq;
                    N[k][q] = s * kp + c * kq;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double pk = N[p][k], qk = N[q][k];
                    N[p][k] = c * pk - s * qk;
                    N[q][k] = s * pk + c * qk;
                }
                for(int k = 0; k < 4; k++)
                {
                    const double kp = V[k][p], kq = V[k][q];
                    V[k][p] = c * kp - s * kq;
                    V[k][q] = s * kp + c * kq;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        best = N[i][i] > N[best][best] ? i : best;
    }
    const glm::quat q{static_cast<float>(V[0][best]), static_cast<float>(V[1][best]), static_cast<float>(V[2][best]),
        static_cast<float>(V[3][best])};
    r.rot = glm::normalize(q);
    r.pos = glm::vec3{cb} - r.rot * glm::vec3{ca};
    return r;
}

} // namespace qvr::rigpose
