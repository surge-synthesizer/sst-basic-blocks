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

#include "catch2.hpp"
#include "sst/basic-blocks/simd/setup.h"
#include "sst/basic-blocks/modulators/FXModControl.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

#include <iostream>

namespace smod = sst::basic_blocks::modulators;

TEST_CASE("Mod LFO Is Well Behaved", "[mod]")
{
    for (int m = smod::FXModControl<32>::mod_sine; m <= smod::FXModControl<32>::mod_square; ++m)
    {
        DYNAMIC_SECTION("Mod Control Bounded For Type " << m)
        {
            for (int tries = 0; tries < 500; ++tries)
            {
                float rate = (float)rand() / (float)RAND_MAX * 10;
                float phase_offset = 0.0999 * (tries % 10);
                float depth = 1.0;

                INFO("200 Samples at " << rate << " with po " << phase_offset);
                smod::FXModControl<32> mc(48000, 1.0 / 48000);
                for (int s = 0; s < 200; ++s)
                {
                    mc.pre_process(m, rate, depth, phase_offset);
                    REQUIRE(mc.value() <= 1.0);
                    REQUIRE(mc.value() >= -1.0);
                }
            }
        }
    }
}

static constexpr int bs{8};
static constexpr double tbs{1.0 / bs};

struct SRProvider
{
    static constexpr double sampleRate{48000};
    static constexpr double samplerate{48000};
    static constexpr double sampleRateInv{1.0 / sampleRate};
    float envelope_rate_linear_nowrap(float f) const { return tbs * sampleRateInv * pow(2.f, -f); }
};

TEST_CASE("SimpleLFO is Bounded")
{
    SRProvider sr;
    using slfo_t = sst::basic_blocks::modulators::SimpleLFO<SRProvider, bs>;

    sst::basic_blocks::dsp::RNG urng;
    for (auto s = (int)slfo_t::SINE; s <= (int)slfo_t::RANDOM_TRIGGER; ++s)
    {
        DYNAMIC_SECTION("Test shape " << s)
        {
            for (auto tries = 0; tries < 500; ++tries)
            {
                urng.reseed(urng.unifU32());
                auto def = urng.unifPM1() * 0.95;
                auto rt = urng.unif01() * 6 - 2;

                auto lfo = slfo_t(&sr, urng);
                lfo.attack((slfo_t::Shape)s);
                for (int i = 0; i < 500; ++i)
                {
                    lfo.process_block(rt, def, (slfo_t::Shape)s);
                    for (int j = 0; j < bs; ++j)
                    {
                        REQUIRE(lfo.outputBlock[j] - 1.0 <= 1e-5);
                        REQUIRE(lfo.outputBlock[j] + 1.0 >= -1e-5);
                    }
                }
            }
        }
    }
}