/*
 * sst-basic-blocks - an open source library of core audio utilities
 * built by Surge Synth Team.
 *
 * Provides a collection of tools useful on the audio thread for blocks,
 * modulation, etc... or useful for adapting code to multiple environments.
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log. Parts of this code are derived from similar
 * functions original in Surge or ShortCircuit.
 *
 * sst-basic-blocks is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html.
 *
 * A very small number of explicitly chosen header files can also be
 * used in an MIT/BSD context. Please see the README.md file in this
 * repo or the comments in the individual files. Only headers with an
 * explicit mention that they are dual licensed may be copied and reused
 * outside the GPL3 terms.
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#include <iostream>
#include <array>

#include "sst/basic-blocks/modulators/SimpleLFO.h"
#include "sst/basic-blocks/tables/TwoToTheXProvider.h"
#include "sst/basic-blocks/dsp/RNG.h"
#include "perfutils.h"

template <int blockSize> struct SRProvider
{
    const sst::basic_blocks::tables::TwoToTheXProvider &ttx;
    SRProvider(const sst::basic_blocks::tables::TwoToTheXProvider &t) : ttx(t) {}
    float envelope_rate_linear_nowrap(float f) const
    {
        return (blockSize * sampleRateInv) * ttx.twoToThe(-f);
    }

    void setSampleRate(double sr)
    {
        samplerate = sr;
        sampleRate = sr;
        sampleRateInv = 1.0 / sr;
    }
    double samplerate{1};
    double sampleRate{1};
    double sampleRateInv{1};
};

template <int blockSize>
void basicTest(sst::basic_blocks::tables::TwoToTheXProvider &ttx, sst::basic_blocks::dsp::RNG &rng)
{
    using srp_t = SRProvider<blockSize>;
    using lfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider<blockSize>, blockSize>;

    auto srp = srp_t(ttx);
    srp.setSampleRate(48000 * 2.5);

    std::array<lfo_t, 8> lfos{
        lfo_t(&srp, rng), lfo_t(&srp, rng), lfo_t(&srp, rng), lfo_t(&srp, rng),
        lfo_t(&srp, rng), lfo_t(&srp, rng), lfo_t(&srp, rng), lfo_t(&srp, rng),
    };

    for (auto sh = lfo_t::SINE; sh <= lfo_t::SH_NOISE; sh = (typename lfo_t::Shape)((int)sh + 1))
        for (auto def : {-0.1f, 0.f, 0.2f})
        {
            {
                auto blocks = (int)(200 * srp.sampleRate / blockSize);
                perf::TimeGuard tg("LFO - shape=" + std::to_string(sh) +
                                       " def=" + std::to_string(def),
                                   __FILE__, __LINE__, 248688);
                for (int lf = 0; lf < lfos.size(); ++lf)
                    lfos[lf].attack(sh);

                for (int i = 0; i < blocks; ++i)
                {
                    for (int lf = 0; lf < lfos.size(); ++lf)
                        lfos[lf].process_block(2.4, def, sh, false, 1.0);
                }
            }
        }
}

void lfoPerformance()
{
    sst::basic_blocks::tables::TwoToTheXProvider ttxlfo;
    sst::basic_blocks::dsp::RNG rng(782567);
    ttxlfo.init();
    std::cout << __FILE__ << ":" << __LINE__ << " LFO Perf starting" << std::endl;
    basicTest<8>(ttxlfo, rng);
}