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
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

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

#include <fstream>
#include <vector>
#include <algorithm>

#define CREATE_FILE 0

static constexpr int tbs{16};
struct SampleSRProvider
{
    double samplerate{44100}, sampleRateInv{1.f / samplerate};
    float envelope_rate_linear_nowrap(float f) const { return tbs * sampleRateInv * pow(2.f, -f); }
} srp;

// assume the last argument is the gate
template <typename T, typename... Args>
std::vector<float> runEnv(T &mod, int gsteps, int rsteps, Args... a)
{
    std::vector<float> res;

    res.push_back(0.f);

    mod.attackFrom(0.f);
    for (int i = 0; i < gsteps; ++i)
    {
        mod.processBlock(a..., true);
        res.push_back(mod.outBlock0);
    }
    for (int i = 0; i < rsteps; ++i)
    {
        mod.processBlock(a..., false);
        res.push_back(mod.outBlock0);
    }

    return res;
}

void dumpFile(const std::vector<float> &a, const std::string &bn)
{
#if CREATE_FILE
    std::ofstream of("/tmp/" + bn + ".csv");
    if (of.is_open())
    {
        int idx{0};
        for (const auto &f : a)
            of << (idx++) << "," << f << "\n";
    }
    of.close();
#endif
}

void dumpMulti(const std::vector<std::vector<float>> &a, const std::string &bn)
{
#if CREATE_FILE
    std::ofstream of("/tmp/" + bn + ".csv");
    if (of.is_open())
    {
        auto mnsz = a[0].size();
        for (const auto &f : a)
            mnsz = std::min(mnsz, f.size());
        for (int i = 0; i < mnsz; ++i)
        {
            of << i;
            for (const auto &f : a)
                of << ", " << f[i];
            of << "\n";
        }
    }
    of.close();
#endif
}

TEST_CASE("Run SC Envelope Some", "[run]")
{
    SECTION("Simple Run")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        auto r = runEnv(ahsc, 150, 80, 0.2, 0, 0.2, 0.5, 0.2, 0, 0, 0);
        dumpFile(r, "simple");
    }

    SECTION("Simple Run with Hold")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        auto r = runEnv(ahsc, 150, 80, 0.2, 0.1, 0.2, 0.5, 0.2, 0, 0, 0);
        dumpFile(r, "simpleHold");
    }

    SECTION("Simple Shape Positive")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        auto r = runEnv(ahsc, 150, 80, 0.2, 0, 0.2, 0.5, 0.2, 0.1, 0.1, 0.1);
        dumpFile(r, "simpleShapePos");
    }

    SECTION("Simple Shape Positive")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        auto r = runEnv(ahsc, 150, 80, 0.2, 0, 0.2, 0.5, 0.2, -0.5, -0.5, -0.5);
        dumpFile(r, "simpleShapeNeg");
    }

    SECTION("Multi Shape Positive")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        std::vector<std::vector<float>> all;
        for (int i = 0; i <= 10; ++i)
        {
            auto r = runEnv(ahsc, 150, 80, 0.2, 0, 0.2, 0.5, 0.2, i * 0.1, i * 0.1, i * 0.1);
            all.push_back(r);
        }
        dumpMulti(all, "multiShapePos");
    }

    SECTION("Multi Shape Negative")
    {
        auto ahsc = sst::basic_blocks::modulators::AHDSRShapedSC<SampleSRProvider, tbs>(&srp);
        std::vector<std::vector<float>> all;
        for (int i = 0; i <= 10; ++i)
        {
            auto r = runEnv(ahsc, 150, 80, 0.2, 0, 0.2, 0.5, 0.2, -i * 0.1, -i * 0.1, -i * 0.1);
            all.push_back(r);
        }
        dumpMulti(all, "multiShapeNeg");
    }
}