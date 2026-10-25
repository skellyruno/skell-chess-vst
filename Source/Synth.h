#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace checkmate
{
constexpr double pi = 3.14159265358979323846;
constexpr int scales[10][8] = {{0, 2, 3, 5, 7, 9, 10, 12},   {0, 2, 3, 5, 7, 8, 10, 12},
                               {0, 2, 3, 5, 7, 8, 11, 12},   {0, 2, 4, 5, 7, 9, 11, 12},
                               {0, 2, 4, 6, 7, 9, 11, 12},   {0, 1, 4, 5, 7, 8, 10, 12},
                               {0, 1, 5, 7, 10, 12, 13, 17}, {0, 3, 5, 6, 7, 10, 12, 15},
                               {0, 4, 7, 8, 11, 12, 15, 16}, {0, 3, 5, 7, 10, 12, 15, 17}};
constexpr const char *scaleNames[] = {
    "Dorian",         "Natural minor", "Harmonic minor", "Major / Ionian",  "Lydian", "Phrygian dominant",
    "Japanese Insen", "Cyberpunk",     "Neo-Riemannian", "Minor pentatonic"};
struct PieceConfig
{
    int wave, octave;
    float filter, attack, release, velocity, pan;
};
constexpr std::array<PieceConfig, 6> defaultPieces = {{{1, 2, .3f, .02f, .28f, .9f, 0},
                                                       {2, 4, .6f, .01f, .45f, 1, -.35f},
                                                       {3, 4, .7f, .12f, .85f, .95f, .35f},
                                                       {0, 2, .45f, .03f, .6f, 1.15f, -.15f},
                                                       {0, 4, .9f, .04f, 1.2f, 1.3f, 0},
                                                       {2, 3, .5f, .08f, 1.4f, 1.05f, 0}}};
inline int squareNote(int s, int piece, int root, int scale, int octave,
                      const std::array<PieceConfig, 6> &cfg)
{
    return std::clamp(
        12 + root + std::clamp(cfg[std::clamp(piece, 0, 5)].octave + octave + (7 - s / 8) / 4, 1, 7) * 12 +
            scales[std::clamp(scale, 0, 9)][s % 8],
        24, 108);
}
struct Settings
{
    float volume = .8f, cutoff = 3400, resonance = 4.5f, drive = .2f, attack = .03f, decay = .3f,
          sustain = .6f, release = .5f;
    float reverb = .35f, reverbDecay = 2.5f, delay = .28f, delayTime = .267f, feedback = .45f, width = .7f,
          grit = .1f, tension = .25f;
    bool tensionMod = true;
    std::array<PieceConfig, 6> pieces = defaultPieces;
};
struct Voice
{
    bool active = false, releasing = false, external = false;
    int note = 0, channel = 1, piece = 0, wave = 0, wait = 0, age = 0, gate = 0, releaseAge = 0;
    double phase = 0, phase2 = 0, delta = 0, delta2 = 0;
    float panL = .707f, panR = .707f;
    float level = 0, releaseLevel = 0, velocity = 0, pan = 0, attack = 0, decay = 0, sustain = 0, release = 0,
          filter = 0;
};
// Fixed voice pool; delay/reverb memory is allocated only during prepare().
class Synth
{
  public:
    Settings settings;
    double rate = 44100;
    std::array<Voice, 96> voices{};
    void prepare(double sr)
    {
        rate = sr;
        delayL.assign(size_t(sr * 2) + 2, 0);
        delayR = delayL;
        write = 0;
        const int lengths[] = {1493, 1601, 1747, 1867};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
            {
                auto &d = room[c * 4 + k];
                d.data.assign(size_t((lengths[k] + c * 79) * sr / 44100) + 1, 0);
                d.pos = 0;
                d.low = 0;
            }
        cachedDecay = -1;
        reset();
    }
    void reset()
    {
        for (auto &v : voices)
            v.active = false;
        std::fill(delayL.begin(), delayL.end(), 0);
        std::fill(delayR.begin(), delayR.end(), 0);
        for (auto &d : room)
        {
            std::fill(d.data.begin(), d.data.end(), 0);
            d.pos = 0;
            d.low = 0;
        }
        ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0;
        smoothedVolume = 0;
    }
    void start(int note, int channel, int piece, float velocity, int delaySamples = 0, bool external = false,
               int action = 0)
    {
        piece = std::clamp(piece, 0, 5);
        auto cfg = settings.pieces[piece];
        Voice *chosen = nullptr;
        for (auto &v : voices)
            if (!v.active)
            {
                chosen = &v;
                break;
            }
        if (!chosen)
            chosen = &*std::min_element(voices.begin(), voices.end(),
                                        [](const Voice &a, const Voice &b) { return a.level < b.level; });
        auto &v = *chosen;
        v = {};
        v.active = true;
        v.note = note;
        v.channel = channel;
        v.piece = piece;
        v.wave = cfg.wave;
        v.external = external;
        v.wait = delaySamples;
        v.velocity = velocity * .16f;
        v.pan = cfg.pan;
        v.attack = std::max(.002f, settings.attack + cfg.attack);
        v.decay = settings.decay;
        v.sustain = settings.sustain;
        v.panL = std::sqrt((1 - std::clamp(v.pan, -1.f, 1.f)) * .5f);
        v.panR = std::sqrt((1 + std::clamp(v.pan, -1.f, 1.f)) * .5f);
        v.delta = std::min(.45, 440 * std::pow(2., (note - 69) / 12.) / rate);
        v.delta2 = std::min(.45, v.delta * (piece == 4 ? 1.004 : piece == 3 ? .5 : 2));
        v.release = std::max(.01f, settings.release + cfg.release);
        v.gate = int(rate * .18);
        v.filter = cfg.filter + (action == 1 ? .5f : 0);
    }
    void off(int note, int channel)
    {
        for (auto &v : voices)
            if (v.active && v.external && v.note == note && v.channel == channel)
                releaseVoice(v);
    }
    void allOff(int channel = 0)
    {
        for (auto &v : voices)
            if (v.active && (!channel || v.channel == channel))
                releaseVoice(v);
    }
    void render(float &left, float &right)
    {
        if (cachedDecay != settings.reverbDecay)
        {
            cachedDecay = settings.reverbDecay;
            for (size_t k = 0; k < room.size(); ++k)
                roomFeedback[k] =
                    std::pow(.001f, float(room[k].data.size()) / float(rate * std::max(.5f, cachedDecay)));
        }
        if (cachedGrit != settings.grit)
        {
            cachedGrit = settings.grit;
            quant = std::pow(2.f, 16 - cachedGrit * 10);
        }
        float l = 0, r = 0, mod = 0;
        for (auto &v : voices)
        {
            if (!v.active)
                continue;
            if (v.wait > 0)
            {
                --v.wait;
                continue;
            }
            if (!v.external && v.age >= v.gate && !v.releasing)
                releaseVoice(v);
            if (v.releasing)
            {
                v.level = v.releaseLevel * (1.f - float(v.releaseAge++) / float(v.release * rate));
                if (v.level <= 0)
                {
                    v.active = false;
                    continue;
                }
            }
            else
            {
                double t = v.age / rate;
                if (t < v.attack)
                    v.level = float(t / v.attack);
                else if (t < v.attack + v.decay)
                    v.level = 1 - (1 - v.sustain) * float((t - v.attack) / v.decay);
                else
                    v.level = v.sustain;
            }
            double dt = v.delta, dt2 = v.delta2;
            if (v.piece == 0)
                dt = std::min(.45, dt * (1 + .05 * std::exp(-double(v.age) / (rate * .012))));
            float a = osc(v.phase, dt, v.wave), b = osc(v.phase2, dt2,
                                                        v.piece == 2   ? 2
                                                        : v.piece == 3 ? 3
                                                                       : v.wave);
            float sample = a;
            if (v.piece == 2)
                sample = .8f * a + .2f * b;
            if (v.piece == 3)
                sample = .8f * a + .2f * b;
            if (v.piece == 4)
                sample = .5f * (a + b);
            v.phase += dt;
            v.phase -= std::floor(v.phase);
            v.phase2 += dt2;
            v.phase2 -= std::floor(v.phase2);
            sample *= v.level * v.velocity;
            l += sample * v.panL;
            r += sample * v.panR;
            mod = std::max(mod, v.filter * v.level);
            ++v.age;
        }
        float target = settings.cutoff * (1 + mod * .8f) *
                       (settings.tensionMod ? 1 + (settings.tension - .25f) * .8f : 1);
        smoothCutoff += (std::clamp(target, 40.f, float(rate * .4)) - smoothCutoff) * .002f;
        float g = std::tan(float(pi) * smoothCutoff / float(rate)),
              k = 1 / std::clamp(settings.resonance, .5f, 20.f);
        float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        auto filter = [&](float x, int c)
        {
            float v3 = x - ic2[c], v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
            ic1[c] = 2 * v1 - ic1[c];
            ic2[c] = 2 * v2 - ic2[c];
            return v2;
        };
        l = filter(l, 0);
        r = filter(r, 1);
        float drive = 1 + settings.drive * 6;
        l = std::tanh(l * drive) / std::sqrt(drive);
        r = std::tanh(r * drive) / std::sqrt(drive);
        if (settings.grit > 0)
        {
            l += (std::round(l * quant) / quant - l) * settings.grit;
            r += (std::round(r * quant) / quant - r) * settings.grit;
        }
        if (!delayL.empty())
        {
            float position = float(write) - std::clamp(settings.delayTime, .02f, 1.9f) * float(rate);
            while (position < 0)
                position += float(delayL.size());
            auto i = size_t(position) % delayL.size(), j = (i + 1) % delayL.size();
            float f = position - std::floor(position);
            float dl = delayL[i] + f * (delayL[j] - delayL[i]), dr = delayR[i] + f * (delayR[j] - delayR[i]);
            float feedback = std::clamp(
                settings.feedback + (settings.tensionMod ? settings.tension * .15f : 0), 0.f, .85f);
            delayL[write] = l + dr * feedback;
            delayR[write] = r + dl * feedback;
            write = (write + 1) % delayL.size();
            l += dl * settings.delay;
            r += dr * settings.delay;
        }
        float wet[2] = {};
        for (int c = 0; c < 2; ++c)
            for (int k2 = 0; k2 < 4; ++k2)
            {
                auto &d = room[c * 4 + k2];
                if (d.data.empty())
                    continue;
                float y = d.data[d.pos];
                d.low = .65f * d.low + .35f * y;
                float fb = roomFeedback[c * 4 + k2];
                d.data[d.pos] = std::tanh((c == 0 ? l : r) * .18f + d.low * fb);
                d.pos = (d.pos + 1) % d.data.size();
                wet[c] += y * .25f;
            }
        l += wet[0] * settings.reverb;
        r += wet[1] * settings.reverb;
        float mid = (l + r) * .5f, side = (l - r) * settings.width * .5f;
        smoothedVolume += (settings.volume - smoothedVolume) * .001f;
        left = std::tanh((mid + side) * smoothedVolume);
        right = std::tanh((mid - side) * smoothedVolume);
    }

  private:
    struct Delay
    {
        std::vector<float> data;
        size_t pos = 0;
        float low = 0;
    };
    std::array<Delay, 8> room;
    std::vector<float> delayL, delayR;
    size_t write = 0;
    std::array<float, 8> roomFeedback{};
    float cachedDecay = -1, cachedGrit = -1, quant = 65536;
    float ic1[2]{}, ic2[2]{}, smoothCutoff = 3400, smoothedVolume = 0;
    static void releaseVoice(Voice &v)
    {
        if (!v.releasing)
        {
            v.releasing = true;
            v.releaseAge = 0;
            v.releaseLevel = v.level;
        }
    }
    static double blep(double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1;
        }
        if (t > 1 - dt)
        {
            t = (t - 1) / dt;
            return t * t + t + t + 1;
        }
        return 0;
    }
    static float osc(double p, double dt, int wave)
    {
        if (wave == 0)
            return float(2 * p - 1 - blep(p, dt));
        if (wave == 1)
            return float((p < .5 ? 1 : -1) + blep(p, dt) - blep(std::fmod(p + .5, 1.), dt));
        // Harmonic-limited triangle avoids corner aliasing near Nyquist.
        if (wave == 2)
        {
            double v = 0;
            for (int n = 1; n <= 15 && n * dt < .48; n += 2)
                v += ((n % 4 == 1) ? 1. : -1.) * std::sin(2 * pi * n * p) / (n * n);
            return float(v * 8 / (pi * pi));
        }
        return float(std::sin(2 * pi * p));
    }
};
} // namespace checkmate
