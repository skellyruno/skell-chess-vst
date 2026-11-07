#include "Synth.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

// Offline synthetic stress test. It does not measure an FL Studio host session.
int main()
{
    constexpr double sr = 48000.0;
    constexpr int samples = 48000;
    std::cout << "test,voices,sample_rate,rendered_ms,wall_ms,block_budget_percent,voice_steals\n";
    for (const int count : {8, 32, 64, 96})
    {
        checkmate::Synth synth;
        synth.prepare(sr);
        synth.settings.volume = .8f;
        synth.settings.reverb = .35f;
        synth.settings.delay = .28f;
        for (int i = 0; i < count; ++i)
            synth.start(36 + i % 48, 1, i % 6, .8f, 0, true);
        double sum = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < samples; ++i)
        {
            float l, r;
            synth.render(l, r);
            if (!std::isfinite(l) || !std::isfinite(r)) throw std::runtime_error("nonfinite sample");
            sum += double(l) * l + double(r) * r;
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        if (sum < 0.0001) throw std::runtime_error("silent benchmark");
        std::cout << "offline_synth," << count << ',' << int(sr) << ',' << samples * 1000.0 / sr
                  << ',' << std::fixed << std::setprecision(2) << elapsed << ',' << elapsed / 10.0
                  << ',' << synth.stolenVoices << '\n';
    }
}
