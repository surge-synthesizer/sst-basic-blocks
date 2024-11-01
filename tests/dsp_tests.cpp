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
#include <cmath>
#include <array>
#include <iostream>

#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"
#include "sst/basic-blocks/dsp/LanczosResampler.h"
#include "sst/basic-blocks/dsp/HilbertTransform.h"
#include "sst/basic-blocks/dsp/FastMath.h"
#include "sst/basic-blocks/dsp/Clippers.h"
#include "sst/basic-blocks/dsp/Lag.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/tables/SincTableProvider.h"
#include "sst/basic-blocks/dsp/SSESincDelayLine.h"
#include "sst/basic-blocks/dsp/FollowSlewAndSmooth.h"
#include "sst/basic-blocks/dsp/OscillatorDriftUnisonCharacter.h"

TEST_CASE("lipol_sse basic", "[dsp]")
{
    SECTION("Block Size 16")
    {
        constexpr int bs{16};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        float where alignas(16)[bs];
        lip.set_target_instant(0.2);
        auto prev = 0.2;
        for (const auto &t : {0.6, 0.2, 0.4})
        {
            lip.set_target(t);
            lip.store_block(where);
            for (int i = 0; i < bs; i++)
            {
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * (i + 1)).margin(1e-5));
            }
            prev = t;
        }
    }

    SECTION("Block Size with FirstRun")
    {
        constexpr int bs{16};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, true>();

        float where alignas(16)[bs];
        auto prev = 0.2;
        for (const auto &t : {0.2, 0.6, 0.2, 0.4})
        {
            lip.set_target(t);
            lip.store_block(where);
            for (int i = 0; i < bs; i++)
            {
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * (i + 1)).margin(1e-5));
            }
            prev = t;
        }
    }

    SECTION("Block Size without FirstRun starts at 0")
    {
        constexpr int bs{16};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        float where alignas(16)[bs];
        auto prev = 0.0;
        for (const auto &t : {0.2, 0.6, 0.2, 0.4})
        {
            lip.set_target(t);
            lip.store_block(where);
            for (int i = 0; i < bs; i++)
            {
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * (i + 1)).margin(1e-5));
            }
            prev = t;
        }
    }

    SECTION("Block Size 32")
    {
        constexpr int bs{32};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        float where alignas(16)[bs];
        lip.set_target_instant(0.2);
        auto prev = 0.2;
        for (const auto &t : {0.6, 0.2, 0.4})
        {
            lip.set_target(t);
            lip.store_block(where);
            for (int i = 0; i < bs; i++)
            {
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * (i + 1)).margin(1e-5));
            }
            prev = t;
        }
    }

    SECTION("Block Size 8")
    {
        constexpr int bs{8};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        float where alignas(16)[bs];
        lip.set_target_instant(0.2);
        auto prev = 0.2;
        for (const auto &t : {0.6, 0.2, 0.4})
        {
            lip.set_target(t);
            lip.store_block(where);
            for (int i = 0; i < bs; i++)
            {
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * (i + 1)).margin(1e-5));
            }
            prev = t;
        }
    }
}

TEST_CASE("lipol_sse multiply_block", "[dsp]")
{
    SECTION("Block Size 16")
    {
        constexpr int bs{16};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        lip.set_target_instant(0.2);
        lip.set_target(0.6);
        float f alignas(16)[bs], r alignas(16)[bs];
        for (int i = 0; i < bs; ++i)
        {
            f[i] = std::sin(i * 0.2 * M_PI);
        }
        lip.multiply_block_to(f, r);
        for (int i = 0; i < bs; i++)
        {
            auto x = (0.2 + (0.6 - 0.2) / bs * (i + 1)) * f[i];
            REQUIRE(x == Approx(r[i]).margin(1e-5));
        }
    }
}

TEST_CASE("lipol_sse fade_block", "[dsp]")
{
    SECTION("Block Size 16")
    {
        constexpr int bs{16};
        auto lip = sst::basic_blocks::dsp::lipol_sse<bs, false>();

        lip.set_target_instant(0.2);
        lip.set_target(0.6);
        float f alignas(16)[bs], g alignas(16)[bs], r alignas(16)[bs];
        for (int i = 0; i < bs; ++i)
        {
            f[i] = std::sin(i * 0.2 * M_PI);
            g[i] = std::cos(i * 0.3 * M_PI);
        }
        lip.fade_blocks(f, g, r);
        for (int i = 0; i < bs; i++)
        {
            auto cx = (0.2 + (0.6 - 0.2) / bs * (i + 1));
            auto rx = f[i] * (1 - cx) + g[i] * cx;
            REQUIRE(rx == Approx(r[i]).margin(1e-5));
        }
    }
}

TEST_CASE("Quadrature Oscillator", "[dsp]")
{
    for (const auto omega : {0.04, 0.12, 0.43, 0.97})
    {
        DYNAMIC_SECTION("Vicanek Quadrature omega=" << omega)
        {
            auto q = sst::basic_blocks::dsp::QuadratureOscillator();

            float p0 = 0;
            q.setRate(omega);
            for (int i = 0; i < 200; ++i)
            {
                REQUIRE(q.v == Approx(sin(p0)).margin(1e-3));
                REQUIRE(q.u == Approx(cos(p0)).margin(1e-3));

                q.step();
                p0 += omega;
            }
        }
    }
}

TEST_CASE("Surge Quadrature Oscillator", "[dsp]")
{
    for (const auto omega : {0.04, 0.12, 0.43, 0.97})
    {
        DYNAMIC_SECTION("Surge Quadrature omega=" << omega)
        {
            auto q = sst::basic_blocks::dsp::SurgeQuadrOsc();

            float p0 = 0;
            q.setRate(omega);
            for (int i = 0; i < 200; ++i)
            {
                REQUIRE(q.r == Approx(sin(p0)).margin(1e-3));
                REQUIRE(q.i == Approx(-cos(p0)).margin(1e-3));

                q.step();
                p0 += omega;
            }
        }
    }
}

TEST_CASE("LanczosResampler", "[dsp]")
{
    SECTION("Input Initializes to Zero")
    {
        sst::basic_blocks::dsp::LanczosResampler<32> lr(48000, 88100);
        auto bs = lr.BUFFER_SZ;
        for (int i = 0; i < 2 * bs; ++i)
        {
            REQUIRE(lr.input[0][i] == 0.f);
            REQUIRE(lr.input[1][i] == 0.f);
        }
    }

    SECTION("Can Interpolate Sine")
    {
        sst::basic_blocks::dsp::LanczosResampler<32> lr(48000, 88100);

        // plot 'lancos_raw.csv' using 1:2 with lines, 'lancos_samp.csv' using 1:2 with lines
        int points = 1000;
        double dp = 1.0 / 370;
        float phase = 0;
        for (auto i = 0; i < points; ++i)
        {
            auto obsS = std::sin(phase * 2.0 * M_PI);
            auto obsR = phase * 2 - 1;
            phase += dp;
            if (phase > 1)
                phase -= 1;
            lr.push(obsS, obsR);
        }

        float outBlock alignas(16)[64], outBlockR alignas(16)[64];
        int gen;
        dp /= 88100.0 / 48000.0;

        phase = 0;
        while ((gen = lr.populateNext(outBlock, outBlockR, 64)) > 0)
        {
            for (int i = 0; i < gen; ++i)
            {
                auto d = outBlock[i] - std::sin(phase * 2.0 * M_PI);
                REQUIRE(fabs(d) < 0.025);
                phase += dp;
            }
        }
    }

    SECTION("441 / 48 up down")
    {
        sst::basic_blocks::dsp::LanczosResampler<32> lUp(44100, 48000);
        sst::basic_blocks::dsp::LanczosResampler<32> lDn(48000, 44100);

        // plot 'lancos_raw.csv' using 1:2 with lines, 'lancos_samp.csv' using 1:2 with lines
        int points = lUp.BUFFER_SZ - 400;
        double dp = 1.0 / 370;
        float phase = 0;
        for (auto i = 0; i < points; ++i)
        {
            auto obsS = std::sin(phase * 2.0 * M_PI);
            auto obsR = phase * 2 - 1;
            phase += dp;
            if (phase > 1)
                phase -= 1;
            lUp.push(obsS, obsR);
        }

        std::cout << lUp.inputsRequiredToGenerateOutputs(1) << std::endl;
        while (lUp.inputsRequiredToGenerateOutputs(1) <= 0)
        {
            float L, R;
            lUp.populateNext(&L, &R, 1);
            lDn.push(L, R);
        }

        phase = 0;
        int spool{0};
        float diff{0};
        float maxDiff{0};
        for (auto i = 0; i < points - 80; ++i)
        {
            float L, R;
            lDn.populateNext(&L, &R, 1);

            if (spool > 10)
            {
                auto expL = std::sin((phase - 2 * dp) * 2.0 * M_PI);

                diff += fabs(expL - L);
                maxDiff = std::max((float)fabs(expL - L), maxDiff);
            }
            spool++;
            phase += dp;
        }
        REQUIRE(diff / (points - 80) < 1e-2);
        REQUIRE(maxDiff < 0.015);
    }
}

TEST_CASE("Check FastMath Functions", "[dsp]")
{
    SECTION("Clamp to -PI,PI")
    {
        for (float f = -2132.7; f < 37424.3; f += 0.741)
        {
            float q = sst::basic_blocks::dsp::clampToPiRange(f);
            INFO("Clamping " << f << " to " << q);
            REQUIRE(q > -M_PI);
            REQUIRE(q < M_PI);
        }
    }

    SECTION("fastSin and fastCos in range -PI, PI")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = sst::basic_blocks::dsp::fastcos(p);
            float fsp = sst::basic_blocks::dsp::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastSin and fastCos in range -10*PI, 10*PI with clampRange")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            p *= 10;
            p = sst::basic_blocks::dsp::clampToPiRange(p);
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = sst::basic_blocks::dsp::fastcos(p);
            float fsp = sst::basic_blocks::dsp::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastSin and fastCos in range -100*PI, 100*PI with clampRange")
    {
        float sds = 0, md = 0;
        int nsamp = 100000;
        for (int i = 0; i < nsamp; ++i)
        {
            float p = 2.f * M_PI * rand() / RAND_MAX - M_PI;
            p *= 100;
            p = sst::basic_blocks::dsp::clampToPiRange(p);
            float cp = std::cos(p);
            float sp = std::sin(p);
            float fcp = sst::basic_blocks::dsp::fastcos(p);
            float fsp = sst::basic_blocks::dsp::fastsin(p);

            float cd = fabs(cp - fcp);
            float sd = fabs(sp - fsp);
            if (cd > md)
                md = cd;
            if (sd > md)
                md = sd;
            sds += cd * cd + sd * sd;
        }
        sds = sqrt(sds) / nsamp;
        REQUIRE(md < 1e-4);
        REQUIRE(sds < 1e-6);
    }

    SECTION("fastTanh and fastTanhSSE")
    {
        for (float x = -4.9; x < 4.9; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = SIMD_MM(set_ps1)(x);
            auto r = sst::basic_blocks::dsp::fasttanhSSE(q);
            auto rn = tanh(x);
            auto rd = sst::basic_blocks::dsp::fasttanh(x);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).epsilon(1e-4));
            REQUIRE(rd == Approx(rn).epsilon(1e-4));
        }

        for (float x = -10; x < 10; x += 0.02)
        {
            INFO("Testing clamped at " << x);
            auto q = SIMD_MM(set_ps1)(x);
            auto r = sst::basic_blocks::dsp::fasttanhSSEclamped(q);
            auto cn = tanh(x);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(cn).epsilon(5e-4));
        }
    }

    SECTION("fastTan")
    {
        // need to bump start point slightly, fasttan is only valid just after -PI/2
        for (float x = -M_PI / 2.0 + 0.001; x < M_PI / 2.0; x += 0.02)
        {
            INFO("Testing fasttan at " << x);
            auto rn = tanf(x);
            auto rd = sst::basic_blocks::dsp::fasttan(x);
            REQUIRE(rd == Approx(rn).epsilon(1e-4));
        }
    }

    SECTION("fastexp and fastexpSSE")
    {
        for (float x = -3.9; x < 2.9; x += 0.02)
        {
            INFO("Testing fastexp at " << x);
            auto q = SIMD_MM(set_ps1)(x);
            auto r = sst::basic_blocks::dsp::fastexpSSE(q);
            auto rn = exp(x);
            auto rd = sst::basic_blocks::dsp::fastexp(x);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = r;

            if (x < 0)
            {
                REQUIRE(U.a[0] == Approx(rn).margin(1e-3));
                REQUIRE(rd == Approx(rn).margin(1e-3));
            }
            else
            {
                REQUIRE(U.a[0] == Approx(rn).epsilon(1e-3));
                REQUIRE(rd == Approx(rn).epsilon(1e-3));
            }
        }
    }

    SECTION("fastSine and fastSinSSE")
    {
        for (float x = -3.14; x < 3.14; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = SIMD_MM(set_ps1)(x);
            auto r = sst::basic_blocks::dsp::fastsinSSE(q);
            auto rn = sin(x);
            auto rd = sst::basic_blocks::dsp::fastsin(x);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).margin(1e-4));
            REQUIRE(rd == Approx(rn).margin(1e-4));
            REQUIRE(U.a[0] == Approx(rd).margin(1e-6));
        }
    }

    SECTION("fastCos and fastCosSSE")
    {
        for (float x = -3.14; x < 3.14; x += 0.02)
        {
            INFO("Testing unclamped at " << x);
            auto q = SIMD_MM(set_ps1)(x);
            auto r = sst::basic_blocks::dsp::fastcosSSE(q);
            auto rn = cos(x);
            auto rd = sst::basic_blocks::dsp::fastcos(x);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).margin(1e-4));
            REQUIRE(rd == Approx(rn).margin(1e-4));
            REQUIRE(U.a[0] == Approx(rd).margin(1e-6));
        }
    }

    SECTION("Clamp to -PI,PI SSE")
    {
        for (float f = -800.7; f < 816.4; f += 0.245)
        {
            auto fs = SIMD_MM(set_ps1)(f);

            auto q = sst::basic_blocks::dsp::clampToPiRangeSSE(fs);
            union
            {
                SIMD_M128 v;
                float a[4];
            } U;
            U.v = q;
            for (int s = 0; s < 4; ++s)
            {
                REQUIRE(U.a[s] == Approx(sst::basic_blocks::dsp::clampToPiRange(f)).margin(1e-4));
            }
        }
    }
}

TEST_CASE("SoftClip", "[dsp]")
{
    float r alignas(16)[4];
    r[0] = -1.6;
    r[1] = -0.8;
    r[2] = 0.6;
    r[3] = 1.7;

    auto v = SIMD_MM(load_ps)(r);
    auto c = sst::basic_blocks::dsp::softclip_ps(v);
    SIMD_MM(store_ps)(r, c);
    REQUIRE(r[0] == Approx(-1.0).margin(0.0001));
    REQUIRE(r[1] == Approx(-0.8 - 4.f / 27.f * pow(-0.8, 3)).margin(0.0001));
    REQUIRE(r[2] == Approx(0.6 - 4.f / 27.f * pow(0.6, 3)).margin(0.0001));
    REQUIRE(r[3] == Approx(1.0).margin(0.0001));
}

TEST_CASE("SoftClip Block", "[dsp]")
{
    float r alignas(16)[32], q alignas(16)[32], h alignas(16)[32], h8 alignas(16)[32],
        t7 alignas(16)[32];
    for (int i = 0; i < 32; ++i)
    {
        r[i] = rand() * 20.4 / RAND_MAX - 10.0;
    }
    sst::basic_blocks::mechanics::copy_from_to<32>(r, q);
    sst::basic_blocks::mechanics::copy_from_to<32>(r, h);
    sst::basic_blocks::mechanics::copy_from_to<32>(r, h8);
    sst::basic_blocks::mechanics::copy_from_to<32>(r, t7);

    sst::basic_blocks::dsp::softclip_block<32>(q);
    sst::basic_blocks::dsp::hardclip_block<32>(h);
    sst::basic_blocks::dsp::hardclip_block8<32>(h8);
    sst::basic_blocks::dsp::tanh7_block<32>(t7);
    for (int i = 0; i < 32; ++i)
    {
        auto sci = std::clamp(r[i], -1.5f, 1.5f);
        auto sc = sci - 4.0 / 27.0 * sci * sci * sci;
        auto hc = std::clamp(r[i], -1.f, 1.f);
        auto hc8 = std::clamp(r[i], -8.f, 8.f);
        REQUIRE(q[i] == Approx(sc).margin(0.00001));
        REQUIRE(h[i] == Approx(hc).margin(0.00001));
        REQUIRE(h8[i] == Approx(hc8).margin(0.00001));
        REQUIRE(t7[i] >= -1);
        REQUIRE(t7[i] <= 1);
    }
}

TEST_CASE("Sinc Delay Line", "[dsp]")
{ // This requires SurgeStorate to initialize its tables.
    // Easiest way to do that is to just make a surge
    SECTION("Test Constants")
    {
        float val = 1.324;
        sst::basic_blocks::tables::SurgeSincTableProvider st;
        sst::basic_blocks::dsp::SSESincDelayLine<4096> dl4096(st.sinctable);

        for (int i = 0; i < 10000; ++i)
        {
            dl4096.write(val);
        }
        for (int i = 0; i < 20000; ++i)
        {
            INFO("Iteration " << i);
            float a = dl4096.read(174.3);
            float b = dl4096.read(1732.4);
            float c = dl4096.read(3987.2);
            float d = dl4096.read(256.0);

            REQUIRE(a == Approx(val).margin(1e-3));
            REQUIRE(b == Approx(val).margin(1e-3));
            REQUIRE(c == Approx(val).margin(1e-3));
            REQUIRE(d == Approx(val).margin(1e-3));

            dl4096.write(val);
        }
    }

    SECTION("Test Ramp")
    {
        float val = 0;
        float dRamp = 0.01;
        sst::basic_blocks::tables::SurgeSincTableProvider st;
        sst::basic_blocks::dsp::SSESincDelayLine<4096> dl4096(st.sinctable);

        for (int i = 0; i < 10000; ++i)
        {
            dl4096.write(val);
            val += dRamp;
        }
        for (int i = 0; i < 20000; ++i)
        {
            INFO("Iteration " << i);
            float a = dl4096.read(174.3);
            float b = dl4096.read(1732.4);
            float c = dl4096.read(3987.2);
            float d = dl4096.read(256.0);

            auto cval = val - dRamp;

            REQUIRE(a == Approx(cval - 174.3 * dRamp).epsilon(1e-3));
            REQUIRE(b == Approx(cval - 1732.4 * dRamp).epsilon(1e-3));
            REQUIRE(c == Approx(cval - 3987.2 * dRamp).epsilon(1e-3));
            REQUIRE(d == Approx(cval - 256.0 * dRamp).epsilon(1e-3));

            float aL = dl4096.readLinear(174.3);
            float bL = dl4096.readLinear(1732.4);
            float cL = dl4096.readLinear(3987.2);
            float dL = dl4096.readLinear(256.0);

            REQUIRE(aL == Approx(cval - 174.3 * dRamp).epsilon(1e-3));
            REQUIRE(bL == Approx(cval - 1732.4 * dRamp).epsilon(1e-3));
            REQUIRE(cL == Approx(cval - 3987.2 * dRamp).epsilon(1e-3));
            REQUIRE(dL == Approx(cval - 256.0 * dRamp).epsilon(1e-3));

            dl4096.write(val);
            val += dRamp;
        }
    }

#if 0
// This prints output I used for debugging
    SECTION( "Generate Output" )
    {
        float dRamp = 0.01;
        SSESincDelayLine<4096> dl4096;

        float v = 0;
        int nsamp = 500;
        for( int i=0; i<nsamp; ++i )
        {
            dl4096.write(v);
            v += dRamp;
        }

        for( int i=100; i<300; ++i )
        {
            auto bi = dl4096.buffer[i];
            auto off = nsamp - i;
            auto bsv = dl4096.read(off - 1);
            auto bsl = dl4096.readLinear(off);

            auto bsvh = dl4096.read(off - 1 - 0.5);
            auto bslh = dl4096.readLinear(off - 0.0 );
            std::cout << off << ", " << bi << ", " << bsv
                      << ", " << bsl << ", " << bi -bsv << ", " << bi-bsl
                      << ", " << bslh << ", " << bi - bslh << std::endl;

            for( int q=0; q<111; ++q )
            {
                std::cout << " " << q << " "
                          << ( dl4096.read(off - q * 0.1) - bi ) / dRamp
                          << " " << ( dl4096.readLinear(off - q * 0.1) - bi ) / dRamp
                          << std::endl;
            }
        }
    }
#endif
}

TEST_CASE("lipol_ps class", "[dsp]")
{
    using lipol_ps = sst::basic_blocks::dsp::lipol_sse<64, false>;
    lipol_ps mypol;
    float prevtarget = -1.0;
    mypol.set_target(prevtarget);
    mypol.instantize();

    constexpr size_t nfloat = 64;
    constexpr size_t nfloat_quad = 16;
    float storeTarget alignas(16)[nfloat];
    assert(mypol.blockSize == nfloat);
    mypol.store_block(storeTarget);

    for (auto i = 0U; i < nfloat; ++i)
        REQUIRE(storeTarget[i] == prevtarget); // should be constant in the first instance

    for (int i = 0; i < 10; ++i)
    {
        float target = (i) * (i) / 100.0;
        mypol.set_target(target);

        mypol.store_block(storeTarget, nfloat_quad);

        REQUIRE(storeTarget[nfloat - 1] == Approx(target));

        float dy = storeTarget[1] - storeTarget[0];
        for (auto j = 1U; j < nfloat; ++j)
        {
            REQUIRE(storeTarget[j] - storeTarget[j - 1] == Approx(dy).epsilon(1e-3));
        }

        REQUIRE(prevtarget + dy == Approx(storeTarget[0]));

        prevtarget = target;
    }
}

TEST_CASE("Hilbert Float", "[dsp]")
{
    SECTION("Hilbert has Positive Frequencies")
    {
        /*
         * This test does the hibert transform of real sin
         * and shows that the angle of the resulting complex
         * tracks only positive frequencies. Which is basically
         * the definition.
         */
        auto sr = 48000;
        sst::basic_blocks::dsp::QuadratureOscillator<float> src, qoP, qoN;
        sst::basic_blocks::dsp::HilbertTransformMonoFloat hilbert;
        hilbert.setSampleRate(sr);

        src.setRate(2.0 * M_PI * 440 / sr);
        qoP.setRate(2.0 * M_PI * 440 / sr);
        qoN.setRate(-2.0 * M_PI * 440 / sr);

        auto accDP{0.f}, accDN{0.f}, dP{0.f}, dN{0.f};
        constexpr int nSteps = 2500, warmUp = 100;
        for (auto i = 0; i < nSteps; ++i)
        {
            auto sC = hilbert.stepComplex(src.u);
            auto qP = std::complex<float>(qoP.u, -qoP.v);
            auto qN = std::complex<float>(qoN.u, -qoN.v);

            auto aC = std::arg(sC);
            auto aP = std::arg(qP);
            auto aN = std::arg(qN);

            if (i == warmUp)
            {
                dP = aC - aP;
                dN = aC - aN;
            }
            if (i > warmUp)
            {
                if (aC < aP)
                {
                    aP -= 2.0 * M_PI;
                }
                if (aC < aN)
                {
                    aN -= 2.0 * M_PI;
                }
                accDN += std::abs(aC - aN - dN);
                accDP += std::abs(aC - aP - dP);
            }
            src.step();
            qoP.step();
            qoN.step();
        }

        accDN /= (nSteps - warmUp);
        accDP /= (nSteps - warmUp);
        REQUIRE(accDP < 0.1);
        REQUIRE(accDN > 1);
    }
}

TEST_CASE("Hilbert SSE", "[dsp]")
{
    SECTION("Hilbert SSE has Positive Frequencies")
    {
        /*
         * This test does the hibert transform of real sin
         * and shows that the angle of the resulting complex
         * tracks only positive frequencies. Which is basically
         * the definition.
         */
        auto sr = 48000;
        sst::basic_blocks::dsp::QuadratureOscillator<float> src, qoP, qoN;
        sst::basic_blocks::dsp::QuadratureOscillator<float> srcR, qoPR, qoNR;
        sst::basic_blocks::dsp::HilbertTransformStereoSSE hilbert;
        hilbert.setSampleRate(sr);

        src.setRate(2.0 * M_PI * 440 / sr);
        qoP.setRate(2.0 * M_PI * 440 / sr);
        qoN.setRate(-2.0 * M_PI * 440 / sr);

        srcR.setRate(2.0 * M_PI * 762 / sr);
        qoPR.setRate(2.0 * M_PI * 762 / sr);
        qoNR.setRate(-2.0 * M_PI * 762 / sr);

        auto accDP{0.f}, accDN{0.f}, dP{0.f}, dN{0.f};
        auto accDPR{0.f}, accDNR{0.f}, dPR{0.f}, dNR{0.f};
        constexpr int nSteps = 2500, warmUp = 100;
        for (auto i = 0; i < nSteps; ++i)
        {
            auto [sC, sR] = hilbert.stepToComplex(src.u, srcR.u);
            auto qP = std::complex<float>(qoP.u, -qoP.v);
            auto qN = std::complex<float>(qoN.u, -qoN.v);

            auto qPR = std::complex<float>(qoPR.u, -qoPR.v);
            auto qNR = std::complex<float>(qoNR.u, -qoNR.v);

            auto aC = std::arg(sC);
            auto aP = std::arg(qP);
            auto aN = std::arg(qN);

            auto aCR = std::arg(sR);
            auto aPR = std::arg(qPR);
            auto aNR = std::arg(qNR);

            if (aC < aP)
            {
                aP -= 2.0 * M_PI;
            }
            if (aC < aN)
            {
                aN -= 2.0 * M_PI;
            }
            if (aCR < aPR)
            {
                aPR -= 2.0 * M_PI;
            }
            if (aCR < aNR)
            {
                aNR -= 2.0 * M_PI;
            }

            if (i == warmUp)
            {
                dP = aC - aP;
                dN = aC - aN;
                dPR = aCR - aPR;
                dNR = aCR - aNR;
            }
            if (i > warmUp)
            {
                accDN += std::abs(aC - aN - dN);
                accDP += std::abs(aC - aP - dP);

                accDNR += std::abs(aCR - aNR - dNR);
                accDPR += std::abs(aCR - aPR - dPR);
            }
            src.step();
            srcR.step();
            qoP.step();
            qoN.step();
            qoPR.step();
            qoNR.step();
        }

        accDN /= (nSteps - warmUp);
        accDP /= (nSteps - warmUp);
        accDNR /= (nSteps - warmUp);
        accDPR /= (nSteps - warmUp);
        REQUIRE(accDP < 0.1);
        REQUIRE(accDN > 1);
        REQUIRE(accDPR < 0.1);
        REQUIRE(accDNR > 1);
    }
}

TEST_CASE("OnePoleLag (aka SurgeLag)", "[dsp]")
{
    SECTION("Basic Default Construct")
    {
        auto l = sst::basic_blocks::dsp::SurgeLag<float, true>();
        // First Run works
        l.newValue(1.f);
        REQUIRE(l.v == 1.f);
        for (auto i = 0; i < 10; ++i)
        {
            l.process();
            REQUIRE(l.v == 1.f);
        }

        // Decreaes is monotonic
        l.newValue(0.9);
        REQUIRE(l.v == 1.f);
        auto pv = l.v;
        for (auto i = 0; i < 10; ++i)
        {
            l.process();
            REQUIRE(l.v < pv);
            pv = l.v;
        }

        // The default speed is fixed. Don't break it
        REQUIRE(l.v == Approx(0.99607f).margin(0.0001f));

        // Turns around
        l.newValue(1.0);
        for (auto i = 0; i < 10; ++i)
        {
            l.process();
            REQUIRE(l.v > pv);
            pv = l.v;
        }
        // Doesn't make it back - its' not linear
        REQUIRE(l.v == Approx(0.99623f).margin(0.0001f));
    }

    SECTION("Speeds Work")
    {
        auto l1 = sst::basic_blocks::dsp::SurgeLag<float, true>(0.05);
        auto l2 = sst::basic_blocks::dsp::SurgeLag<float, true>(0.07);
        // First Run works
        l1.newValue(1.f);
        l2.newValue(1.f);
        REQUIRE(l1.v == 1.f);
        REQUIRE(l2.v == 1.f);

        l1.newValue(0.9);
        l2.newValue(0.9);
        for (auto i = 0; i < 100; ++i)
        {
            l1.process();
            l2.process();
            REQUIRE(l1.v > l2.v);
        }
    }

    SECTION("Instantize Works")
    {
        auto l1 = sst::basic_blocks::dsp::SurgeLag<float, true>(0.05);
        // First Run works
        l1.newValue(1.f);
        REQUIRE(l1.v == 1.f);

        l1.newValue(0.9);
        auto pv = l1.v;
        for (auto i = 0; i < 100; ++i)
        {
            l1.process();
            REQUIRE(l1.v < pv);
            pv = l1.v;
        }

        l1.newValue(0.7);
        l1.process();
        REQUIRE(l1.v != 0.7f);
        l1.instantize();
        REQUIRE(l1.v == 0.7f);
    }

    SECTION("Name Alias in place")
    {
        auto l1 = sst::basic_blocks::dsp::OnePoleLag<float, true>(0.05);
        l1.snapTo(1.f);
        REQUIRE(l1.v == 1.f);
    }

    SECTION("Time in miliseconds is basicaly OK")
    {
        for (auto time : {20, 50, 100, 500})
        {
            for (auto sr : {44100, 48000, 88200, 96000})
            {
                for (auto bs : {8, 16, 32})
                {
                    INFO("Lag Test " << time << " ms " << sr << " sr " << bs << " block");
                    auto l1 = sst::basic_blocks::dsp::OnePoleLag<float, true>();

                    l1.setRateInMilliseconds(time, sr, 1.0 / bs);
                    l1.snapTo(0.f);
                    l1.setTarget(1.f);

                    auto requiredIts = (time / 1000.0) * sr / bs;
                    // So 100ms at 48k with 16 block size is .1 * 48k / 16 = 300
                    for (int i = 0; i < requiredIts / 2; ++i)
                    {
                        l1.process();
                        REQUIRE(l1.v > 0);
                        REQUIRE(l1.v < 1.);
                    }

                    // Most of the way there in half the time
                    REQUIRE(l1.v > 0.9);
                    REQUIRE(l1.v < 0.96);

                    for (int i = 0; i < requiredIts / 2; ++i)
                    {
                        l1.process();
                        REQUIRE(l1.v > 0);
                        REQUIRE(l1.v < 1.);
                    }
                    REQUIRE(l1.v == Approx(1.f).margin(5e-3));
                }
            }
        }
    }
}

TEST_CASE("LinearLag", "[dsp]")
{
    SECTION("Time in miliseconds is basicaly OK")
    {
        for (auto time : {20, 50, 100, 500})
        {
            for (auto sr : {44100, 48000, 88200, 96000})
            {
                for (auto bs : {8, 16, 32})
                {
                    INFO("Lag Test " << time << " ms " << sr << " sr " << bs << " block");
                    auto l1 = sst::basic_blocks::dsp::LinearLag<float, true>();

                    l1.setRateInMilliseconds(time, sr, 1.0 / bs);
                    l1.snapTo(0.f);
                    l1.setTarget(1.f);

                    auto requiredIts = (time / 1000.0) * sr / bs;
                    // So 100ms at 48k with 16 block size is .1 * 48k / 16 = 300
                    for (int i = 0; i < requiredIts / 2; ++i)
                    {
                        l1.process();
                        REQUIRE(l1.v > 0);
                        REQUIRE(l1.v < 1.);
                    }

                    // Half of the way there in half the time
                    REQUIRE(l1.v > 0.49);
                    REQUIRE(l1.v < 0.51);

                    for (int i = 0; i < requiredIts / 2 + 1; ++i)
                    {
                        l1.process();
                        REQUIRE(l1.v > 0.5);
                        REQUIRE(l1.v <= 1.);
                    }
                    REQUIRE(l1.v == Approx(1.f).margin(5e-3));
                }
            }
        }
    }
}
TEST_CASE("UIComponentLagHandler", "[dsp]")
{
    SECTION("Basics Up")
    {
        sst::basic_blocks::dsp::UIComponentLagHandler lag;

        lag.setRate(120, 16, 48000);
        float f{0};
        lag.setNewDestination(&f, 0.5);
        REQUIRE(f == 0);
        REQUIRE(lag.active);
        lag.process();
        REQUIRE(f > 0);
        auto fp = f;
        int its{0};
        while (lag.active && its++ < 100)
        {
            lag.process();
            REQUIRE(fp <= f);
            fp = f;
            REQUIRE(f <= 0.5);
        }
        REQUIRE(!lag.active);
        REQUIRE(its < 100);
        REQUIRE(f == 0.5);
    }

    SECTION("Basics Down")
    {
        sst::basic_blocks::dsp::UIComponentLagHandler lag;

        lag.setRate(120, 16, 48000);
        float f{1.0};
        lag.setNewDestination(&f, 0.5);
        REQUIRE(f == 1.0);
        REQUIRE(lag.active);
        lag.process();
        REQUIRE(f < 1.0);
        auto fp = f;
        int its{0};
        while (lag.active && its++ < 100)
        {
            lag.process();
            REQUIRE(fp >= f);
            fp = f;
            REQUIRE(f >= 0.5);
        }
        REQUIRE(!lag.active);
        REQUIRE(its < 100);
        REQUIRE(f == 0.5);
    }

    SECTION("Instant")
    {
        sst::basic_blocks::dsp::UIComponentLagHandler lag;

        lag.setRate(120, 16, 48000);
        float f{0};
        lag.setNewDestination(&f, 0.5);
        REQUIRE(f == 0);
        REQUIRE(lag.active);
        lag.instantlySnap();
        REQUIRE(f == 0.5);
        REQUIRE(!lag.active);
    }

    SECTION("Interrupt")
    {
        sst::basic_blocks::dsp::UIComponentLagHandler lag;

        lag.setRate(120, 16, 48000);
        float f{0};
        float g{1};
        lag.setNewDestination(&f, 0.5);
        REQUIRE(f == 0);
        for (int i = 0; i < 4; ++i)
            lag.process();
        REQUIRE(f < 0.5);
        REQUIRE(lag.active);
        lag.setNewDestination(&g, 0.7);
        REQUIRE(f == 0.5);
        lag.process();
        REQUIRE(g < 1.0);
    }
}

TEST_CASE("Slew", "[dsp]")
{
    auto sl = sst::basic_blocks::dsp::SlewLimiter();
    sl.setParams(100, 1.0, 1000);

    for (int i = 0; i < 100; ++i)
    {
        auto val = sl.step(0.5);
        if (i < 50)
            REQUIRE(val == Approx((i + 1) * 0.01));
        else
            REQUIRE(val == 0.5);
    }

    for (int i = 0; i < 200; ++i)
    {
        auto val = sl.step(-0.5);
        if (i < 100)
            REQUIRE(val == Approx(0.5 - (i + 1) * 0.01).margin(1e-5));
        else
            REQUIRE(val == -0.5);
    }
}

TEST_CASE("Running Avg", "[dsp]")
{
    SECTION("Constants")
    {
        std::array<float, 1000> data{};
        auto ra = sst::basic_blocks::dsp::RunningAverage();
        ra.setStorage(data.data(), data.size());
        for (int i = 0; i < data.size() - 1; ++i)
        {
            auto val = ra.step(3.2);
            REQUIRE(val == Approx(3.2 * (i + 1) / 1000.0).margin(0.005));
        }
    }

    SECTION("RAMP")
    {
        std::array<float, 101> data{};
        auto ra = sst::basic_blocks::dsp::RunningAverage();
        ra.setStorage(data.data(), data.size());
        for (int i = 0; i < 500; ++i)
        {
            auto val = ra.step(i * 0.1);
            if (i > data.size() - 1)
            {
                // Filled with a ramp. Average is start - end / count
                auto avg = (i + (i - (data.size() - 1 - 1))) * 0.5 * 0.1;
                REQUIRE(val == Approx(avg).margin(0.005));
            }
        }
    }
}

TEST_CASE("OscillatorSupportFunctions", "[dsp]")
{
    SECTION("Drift LFO")
    {
        auto dfo = sst::basic_blocks::dsp::DriftLFO();
        dfo.init(true);
        REQUIRE(dfo.val() == 0.f);

        REQUIRE(dfo.next() != 0.f);
    }

    SECTION("Unison")
    {
        auto us = sst::basic_blocks::dsp::UnisonSetup<float>(3);
        float L, R;
        us.panLaw(0, L, R);
        REQUIRE(L < R);
        us.panLaw(1, L, R);
        REQUIRE(L == Approx(R));
        us.panLaw(2, L, R);
        REQUIRE(L > R);
    }
}