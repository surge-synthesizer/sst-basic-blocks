//
// Created by Paul Walker on 2/8/23.
//

// For now just a simple smoke test


#if defined(__arm64__)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#else
#include <emmintrin.h>
#endif

#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "sst/basic-blocks/dsp/Interpolators.h"

#include "sst/basic-blocks/modulators/ADAREnvelope.h"
#include "sst/basic-blocks/modulators/ADSREnvelope.h"
#include "sst/basic-blocks/modulators/DAHDEnvelope.h"
#include "sst/basic-blocks/modulators/SimpleLFO.h"

#include <memory>

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
    adsr.process(0,0,0,0,0,0,0,0);
    auto dahd = sst::basic_blocks::modulators::DAHDEnvelope<SampleSRProvider, 16>(srp.get());
    adsr.process(0,0,0,0,0,0,0,0);
    auto ad = sst::basic_blocks::modulators::ADAREnvelope<SampleSRProvider, 16>(srp.get());
    ad.processScaledAD(0,0,0,0,false);
    auto lf = sst::basic_blocks::modulators::SimpleLFO<SampleSRProvider, 16>(srp.get());
    lf.process_block(0,0,0);

}