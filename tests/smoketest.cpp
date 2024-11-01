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

// For now just a simple smoke test

#define CATCH_CONFIG_RUNNER
#include "catch2.hpp"

#include <iostream>
#include "sst/basic-blocks/simd/setup.h"
#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/basic-blocks/dsp/Interpolators.h"
#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/PanLaws.h"

#include "sst/basic-blocks/modulators/ADAREnvelope.h"
#include "sst/basic-blocks/modulators/ADSREnvelope.h"
#include "sst/basic-blocks/modulators/DAHDEnvelope.h"
#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

#include "sst/basic-blocks/dsp/RNG.h"

#include <memory>
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/endian-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"

int main(int argc, char **argv)
{
    using namespace sst::basic_blocks;

    struct SampleSRProvider
    {
        double samplerate{44100};
        float envelope_rate_linear_nowrap(float f) const { return 0; }
    };

    auto srp = std::make_unique<SampleSRProvider>();
    auto adsr = sst::basic_blocks::modulators::ADSREnvelope<SampleSRProvider, 16>(srp.get());
    adsr.process(0, 0, 0, 0, 0, 0, 0, 0);
    auto dahd = sst::basic_blocks::modulators::DAHDEnvelope<SampleSRProvider, 16>(srp.get());
    adsr.process(0, 0, 0, 0, 0, 0, 0, 0);
    auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, 16>(srp.get());
    ahsc.processBlock(0, 0, 0, 0, 0, 0, 0, 0, false, true);
    auto ad = sst::basic_blocks::modulators::ADAREnvelope<SampleSRProvider, 16>(srp.get());
    ad.processScaledAD(0, 0, 0, 0, false);
    auto lf = sst::basic_blocks::modulators::SimpleLFO<SampleSRProvider, 16>(srp.get());
    lf.process_block(0, 0, 0);

    sst::basic_blocks::dsp::RNG rngA, rngB(2112);
    auto lfer = sst::basic_blocks::modulators::SimpleLFO<SampleSRProvider, 16>(srp.get(), rngB);
    lfer.process_block(0, 0, 0);

    float f[8], g[8];
    sst::basic_blocks::mechanics::accumulate_from_to<8>(f, g);
    sst::basic_blocks::mechanics::copy_from_to<8>(f, g);
    sst::basic_blocks::mechanics::endian_read_int16LE((uint16_t)138);

    int result = Catch::Session().run(argc, argv);

    return result;
}
