//
// Created by Paul Walker on 2/8/23.
//

// For now just a simple smoke test

#define CATCH_CONFIG_RUNNER
#include "catch2.hpp"

#include <iostream>
#include "smoke_test_sse.h"

#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/basic-blocks/dsp/Interpolators.h"
#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/PanLaws.h"

#include "sst/basic-blocks/modulators/ADAREnvelope.h"
#include "sst/basic-blocks/modulators/ADSREnvelope.h"
#include "sst/basic-blocks/modulators/DAHDEnvelope.h"
#include "sst/basic-blocks/modulators/AHDSRShapedSC.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

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
    ahsc.processBlock(0, 0, 0, 0, 0, 0, 0, 0, false);
    auto ad = sst::basic_blocks::modulators::ADAREnvelope<SampleSRProvider, 16>(srp.get());
    ad.processScaledAD(0, 0, 0, 0, false);
    auto lf = sst::basic_blocks::modulators::SimpleLFO<SampleSRProvider, 16>(srp.get());
    lf.process_block(0, 0, 0);

    float f[8], g[8];
    sst::basic_blocks::mechanics::accumulate_from_to<8>(f, g);
    sst::basic_blocks::mechanics::copy_from_to<8>(f, g);
    sst::basic_blocks::mechanics::endian_read_int16LE((uint16_t)138);

    int result = Catch::Session().run(argc, argv);

    return result;
}