// vr_still.cpp -- see vr_still.hpp.

#include "vr_still.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"


namespace qvr::still
{

float degreesBetween(const glm::vec3& a, const glm::vec3& b)
{
    const float la = glm::length(a), lb = glm::length(b);
    if(la < 1e-6f || lb < 1e-6f)
    {
        return 0.f;
    }
    return glm::degrees(za::acos(za::fmax(-1.f, za::fmin(1.f, glm::dot(a, b) / (la * lb)))));
}

// ---------------------------------------------------------------------------------------------------------------------

void Countdown::start(double now, double seconds)
{
    started = now;
    length = seconds;
    beeps = 0;
    active = true;
    done = false;
}

bool Countdown::update(double now)
{
    if(done)
    {
        return true;
    }
    if(!active)
    {
        return false;
    }
    const int total = za::max(1, static_cast<int>(za::lround(length)));
    const int due = static_cast<int>((now - started) / (length / total)) + 1;
    while(beeps < za::min(due, total))
    {
        S_LocalSound("misc/menu1.wav");
        beeps++;
    }
    if(now - started >= length)
    {
        S_LocalSound("misc/menu2.wav"); // go
        done = true;
        active = false;
    }
    return done;
}

int Countdown::remaining(double now) const
{
    if(!active)
    {
        return 0;
    }
    return za::max(1, static_cast<int>(za::ceil(length - (now - started))));
}

// ---------------------------------------------------------------------------------------------------------------------

Window::Window(za::Vector<Channel> channels) : spec(ZA_MOVE(channels)), devs(spec.size(), 0.f)
{
}

void Window::clear()
{
    times.clear();
    values.clear();
}

void Window::add(double time, const glm::vec3* v, double keep)
{
    if(!times.empty() && times.back() == time)
    {
        return; // the second eye's pass
    }
    times.pushBack(time);
    values.emplaceBackRange(v, spec.size());
    size_t drop = 0;
    while(drop < times.size() && time - times[drop] > keep)
    {
        drop++;
    }
    if(drop)
    {
        times.erase(times.begin(), times.begin() + static_cast<za::PtrDiffT>(drop));
        values.erase(values.begin(), values.begin() + static_cast<za::PtrDiffT>(drop * spec.size()));
    }
}

glm::vec3 Window::last(int c) const
{
    return times.empty() ? glm::vec3{0.f} : values[(times.size() - 1) * spec.size() + static_cast<size_t>(c)];
}

bool Window::still(double now, double seconds, za::Vector<glm::vec3>& mean, int minSamples)
{
    const size_t nc = spec.size();
    qza::fill(devs.begin(), devs.end(), 0.f);
    if(times.empty() || now - times.front() < seconds - 1e-3)
    {
        return false;
    }
    za::Vector<glm::vec3> m(nc, glm::vec3{0.f});
    int n = 0;
    for(size_t i = 0; i < times.size(); i++)
    {
        if(now - times[i] <= seconds + 1e-3)
        {
            for(size_t c = 0; c < nc; c++)
            {
                m[c] += values[i * nc + c];
            }
            n++;
        }
    }
    if(n < minSamples)
    {
        return false;
    }
    const float k = 1.f / static_cast<float>(n);
    for(size_t c = 0; c < nc; c++)
    {
        const bool dir = spec[c].kind == Kind::Direction || spec[c].kind == Kind::AveragedDirection;
        m[c] = dir ? (glm::length(m[c]) > 1e-6f ? glm::normalize(m[c]) : m[c]) : m[c] * k;
    }
    bool ok = true;
    for(size_t i = 0; i < times.size(); i++)
    {
        if(now - times[i] > seconds + 1e-3)
        {
            continue;
        }
        for(size_t c = 0; c < nc; c++)
        {
            const Channel& ch = spec[c];
            if(ch.kind == Kind::Averaged || ch.kind == Kind::AveragedDirection)
            {
                continue;
            }
            const glm::vec3& v = values[i * nc + c];
            const float d = ch.kind == Kind::Direction ? degreesBetween(v, m[c]) : glm::distance(v, m[c]);
            devs[c] = za::fmax(devs[c], d);
            ok = ok && d <= ch.tolerance;
        }
    }
    if(ok)
    {
        mean = ZA_MOVE(m);
    }
    return ok;
}

} // namespace qvr::still
