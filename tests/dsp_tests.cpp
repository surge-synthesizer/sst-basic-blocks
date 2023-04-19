//
// Created by Paul Walker on 4/3/23.
//

#include "catch2.hpp"
#include "smoke_test_sse.h"
#include <cmath>
#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"
#include "sst/basic-blocks/dsp/LanczosResampler.h"
#include "sst/basic-blocks/dsp/FastMath.h"
#include "sst/basic-blocks/dsp/Clippers.h"
#include "sst/basic-blocks/mechanics/block-ops.h"

#include <iostream>

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
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * i).margin(1e-5));
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
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * i).margin(1e-5));
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
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * i).margin(1e-5));
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
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * i).margin(1e-5));
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
                REQUIRE(where[i] == Approx(prev + (t - prev) / bs * i).margin(1e-5));
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
            auto x = (0.2 + (0.6 - 0.2) / bs * i) * f[i];
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
            auto cx = (0.2 + (0.6 - 0.2) / bs * i);
            auto rx = f[i] * (1 - cx) + g[i] * cx;
            REQUIRE(rx == Approx(r[i]).margin(1e-5));
        }
    }
}

TEST_CASE("Quadrature Oscillator")
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

TEST_CASE("LanczosResampler", "[dsp]")
{
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
        int q, gen;
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
            auto q = _mm_set_ps1(x);
            auto r = sst::basic_blocks::dsp::fasttanhSSE(q);
            auto rn = tanh(x);
            auto rd = sst::basic_blocks::dsp::fasttanh(x);
            union
            {
                __m128 v;
                float a[4];
            } U;
            U.v = r;
            REQUIRE(U.a[0] == Approx(rn).epsilon(1e-4));
            REQUIRE(rd == Approx(rn).epsilon(1e-4));
        }

        for (float x = -10; x < 10; x += 0.02)
        {
            INFO("Testing clamped at " << x);
            auto q = _mm_set_ps1(x);
            auto r = sst::basic_blocks::dsp::fasttanhSSEclamped(q);
            auto cn = tanh(x);
            union
            {
                __m128 v;
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
            auto q = _mm_set_ps1(x);
            auto r = sst::basic_blocks::dsp::fastexpSSE(q);
            auto rn = exp(x);
            auto rd = sst::basic_blocks::dsp::fastexp(x);
            union
            {
                __m128 v;
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
            auto q = _mm_set_ps1(x);
            auto r = sst::basic_blocks::dsp::fastsinSSE(q);
            auto rn = sin(x);
            auto rd = sst::basic_blocks::dsp::fastsin(x);
            union
            {
                __m128 v;
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
            auto q = _mm_set_ps1(x);
            auto r = sst::basic_blocks::dsp::fastcosSSE(q);
            auto rn = cos(x);
            auto rd = sst::basic_blocks::dsp::fastcos(x);
            union
            {
                __m128 v;
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
            auto fs = _mm_set_ps1(f);

            auto q = sst::basic_blocks::dsp::clampToPiRangeSSE(fs);
            union
            {
                __m128 v;
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

    auto v = _mm_load_ps(r);
    auto c = sst::basic_blocks::dsp::softclip_ps(v);
    _mm_store_ps(r, c);
    REQUIRE(r[0] == Approx(-1.0).margin(0.0001));
    REQUIRE(r[1] == Approx(-0.8 - 4.f / 27.f * pow(-0.8, 3)).margin(0.0001));
    REQUIRE(r[2] == Approx(0.6 - 4.f / 27.f * pow(0.6, 3)).margin(0.0001));
    REQUIRE(r[3] == Approx(1.0).margin(0.0001));
}

TEST_CASE("SoftClip Block", "[dsp]")
{
    float r alignas(16)[32],
        q alignas(16)[32],
        h alignas(16)[32],
        h8 alignas(16)[32],
        t7 alignas(16)[32];
    for (int i = 0; i < 32; ++i)
    {
        r[i] = rand() * 20.4 / RAND_MAX - 10.0;
    }
    sst::basic_blocks::mechanics::copy_from_to<32>(r,q);
    sst::basic_blocks::mechanics::copy_from_to<32>(r,h);
    sst::basic_blocks::mechanics::copy_from_to<32>(r,h8);
    sst::basic_blocks::mechanics::copy_from_to<32>(r,t7);

    sst::basic_blocks::dsp::softclip_block<32>(q);
    sst::basic_blocks::dsp::hardclip_block<32>(h);
    sst::basic_blocks::dsp::hardclip_block8<32>(h8);
    sst::basic_blocks::dsp::tanh7_block<32>(t7);
    for (int i=0; i<32; ++i)
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
