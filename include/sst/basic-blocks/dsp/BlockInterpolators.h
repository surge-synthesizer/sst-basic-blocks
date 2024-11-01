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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_BLOCKINTERPOLATORS_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_BLOCKINTERPOLATORS_H

#include <cassert>
#include "sst/basic-blocks/simd/setup.h"

namespace sst::basic_blocks::dsp
{
template <class T, int defaultBlockSize, bool first_run_checks> struct lipol
{
  public:
    lipol() { reset(); }
    void reset()
    {
        if (first_run_checks)
            first_run = true;
        new_v = 0;
        v = 0;
        dv = 0;
    }
    inline void newValue(T f)
    {
        v = new_v;
        new_v = f;
        if (first_run_checks && first_run)
        {
            v = f;
            first_run = false;
        }
        dv = (new_v - v) * bs_inv;
    }
    inline void instantize()
    {
        v = new_v;
        dv = (T)0;
    }
    inline T getTargetValue() { return new_v; }
    inline void process() { v += dv; }
    /*
     * Some clients specify strictly in the template; others do not. Make it so the
     * template is the default but not the requirement.
     */
    inline void setBlockSize(int bsOverride) { bs_inv = 1 / (T)bsOverride; }
    T v;

    static constexpr T bs_inv_def{(T)1 / (T)defaultBlockSize};
    T new_v{0};
    T dv{0};
    T bs_inv{bs_inv_def};
    bool first_run{true};
};

template <int maxBlockSize, bool first_run_checks = true> struct alignas(16) lipol_sse
{
  private:
    // put these at the top to preserve alignment
    SIMD_M128 line[maxBlockSize >> 2];
    SIMD_M128 zeroUpByQuarters;
    SIMD_M128 one, zero;

  public:
    static constexpr int maxRegisters{maxBlockSize >> 2};
    int numRegisters{maxBlockSize >> 2};
    int blockSize{maxBlockSize};
    float blockSizeInv{1.f / blockSize};
    float registerSizeInv{1.f / (blockSize >> 2)};

    float target{0.f}, current{0.f};
    bool first_run{true};

  public:
    static_assert(!(maxBlockSize & (maxBlockSize - 1)));

    lipol_sse()
    {
        float zbq alignas(16)[4]{0.25f, 0.5f, 0.75f, 1.00f};
        zeroUpByQuarters = SIMD_MM(load_ps)(zbq);
        one = SIMD_MM(set1_ps)(1.f);
        zero = SIMD_MM(setzero_ps)();
    }
    void set_target(float f)
    {
        current = target;
        target = f;
        if constexpr (first_run_checks)
        {
            if (first_run)
            {
                first_run = false;
                current = f;
            }
        }
        updateLine();
    }

    void set_target_smoothed(float f)
    {
        constexpr float coef = 0.25;
        constexpr float coef_m1 = 1 - coef;
        current = target;
        auto p1 = coef * f;
        auto p2 = coef_m1 * target;
        target = p1 + p2;
        updateLine();
    }

    inline void instantize() { set_target_instant(target); }
    void set_target_instantize(float f) { set_target_instant(f); }
    void set_target_instant(float f)
    {
        target = f;
        current = f;
        updateLine();
    }
    float get_target() const { return target; }

    /*
     * Out = in * linearly-interpolated-target.
     *
     * When porting from Surge, surge made the block size explicit. That's a useful test
     * that the port is correct so for now we add a block size quad argument to these
     * and assert that they are correct.
     */
    void multiply_block_to(const float *__restrict const in, float *__restrict out,
                           int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        for (int i = 0; i < numRegisters; ++i)
        {
            auto iv = SIMD_MM(load_ps)(in + (i << 2));
            auto ov = SIMD_MM(mul_ps)(iv, line[i]);
            SIMD_MM(store_ps)(out + (i << 2), ov);
        }
    }

    void multiply_block(float *in, int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        for (int i = 0; i < numRegisters; ++i)
        {
            auto iv = SIMD_MM(load_ps)(in + (i << 2));
            auto ov = SIMD_MM(mul_ps)(iv, line[i]);
            SIMD_MM(store_ps)(in + (i << 2), ov);
        }
    }

    void multiply_2_blocks(float *__restrict in1, float *__restrict in2, int bsQuad = -1) const
    {
        multiply_block(in1, bsQuad);
        multiply_block(in2, bsQuad);
    }

    void multiply_2_blocks_to(const float *__restrict const inL, const float *__restrict const inR,
                              float *__restrict outL, float *__restrict outR, int bsQuad = -1) const
    {
        multiply_block_to(inL, outL, bsQuad);
        multiply_block_to(inR, outR, bsQuad);
    }

    /*
     * MAC means "multiply-accumulate"
     */
    void MAC_block_to(float *__restrict src, float *__restrict dst, int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        for (int i = 0; i < numRegisters; ++i)
        {
            auto iv = SIMD_MM(load_ps)(src + (i << 2));
            auto dv = SIMD_MM(load_ps)(dst + (i << 2));
            auto ov = SIMD_MM(mul_ps)(iv, line[i]);
            auto mv = SIMD_MM(add_ps)(ov, dv);
            SIMD_MM(store_ps)(dst + (i << 2), mv);
        }
    }
    void MAC_2_blocks_to(float *__restrict src1, float *__restrict src2, float *__restrict dst1,
                         float *__restrict dst2, int bsQuad = -1) const
    {
        MAC_block_to(src1, dst1, bsQuad);
        MAC_block_to(src2, dst2, bsQuad);
    }

    /*
     * out = a * (1-t) + b * t
     */
    void fade_blocks(float *__restrict inA, float *__restrict inB, float *__restrict out) const
    {
        for (int i = 0; i < numRegisters; ++i)
        {
            auto a = SIMD_MM(load_ps)(inA + (i << 2));
            auto b = SIMD_MM(load_ps)(inB + (i << 2));
            auto sa = SIMD_MM(mul_ps)(a, SIMD_MM(sub_ps)(one, line[i]));
            auto sb = SIMD_MM(mul_ps)(b, line[i]);
            auto r = SIMD_MM(add_ps)(sa, sb);
            SIMD_MM(store_ps)(out + (i << 2), r);
        }
    }

    void fade_block_to(float *__restrict src1, float *__restrict src2, float *__restrict dst,
                       int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        fade_blocks(src1, src2, dst);
    }
    void fade_2_blocks_to(float *__restrict src11, float *__restrict src12, float *__restrict src21,
                          float *__restrict src22, float *__restrict dst1, float *__restrict dst2,
                          int bsQuad = -1) const
    {
        fade_block_to(src11, src12, dst1, bsQuad);
        fade_block_to(src21, src22, dst2, bsQuad);
    }

    void fade_blocks_inplace(float *__restrict inAOut, float *__restrict inB) const
    {
        for (int i = 0; i < numRegisters; ++i)
        {
            auto a = SIMD_MM(load_ps)(inAOut + (i << 2));
            auto b = SIMD_MM(load_ps)(inB + (i << 2));
            auto sa = SIMD_MM(mul_ps)(a, SIMD_MM(sub_ps)(one, line[i]));
            auto sb = SIMD_MM(mul_ps)(b, line[i]);
            auto r = SIMD_MM(add_ps)(sa, sb);
            SIMD_MM(store_ps)(inAOut + (i << 2), r);
        }
    }

    void fade_2_blocks_inplace(float *__restrict src11out, float *__restrict src12,
                               float *__restrict src21out, float *__restrict src22,
                               int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        fade_blocks_inplace(src11out, src12);
        fade_blocks_inplace(src21out, src22);
    }

    void store_block(float *__restrict out, int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);
        for (int i = 0; i < numRegisters; ++i)
        {
            SIMD_MM(store_ps)(out + (i << 2), line[i]);
        }
    }

    /*
     * trixpan blocks:
     * a = max(line, 0)
     * b = min(line, o)
     * tl = (1-a) * L - b * R
     * tR = a * L + (1+b) * R
     */
    void trixpan_blocks(float *__restrict L, float *__restrict R, float *__restrict dL,
                        float *__restrict dR, int bsQuad = -1) const
    {
        assert(bsQuad == -1 || bsQuad == numRegisters);

        for (int i = 0; i < numRegisters; ++i)
        {
            auto a = SIMD_MM(max_ps)(zero, line[i]);
            auto b = SIMD_MM(min_ps)(zero, line[i]);
            auto l = SIMD_MM(load_ps)(L + (i << 2));
            auto r = SIMD_MM(load_ps)(R + (i << 2));
            auto tl =
                SIMD_MM(sub_ps)(SIMD_MM(mul_ps)(SIMD_MM(sub_ps)(one, a), l), SIMD_MM(mul_ps)(b, r));
            auto tr =
                SIMD_MM(add_ps)(SIMD_MM(mul_ps)(a, l), SIMD_MM(mul_ps)(SIMD_MM(add_ps)(one, b), r));
            SIMD_MM(store_ps)(dL + (i << 2), tl);
            SIMD_MM(store_ps)(dR + (i << 2), tr);
        }
    }

    void set_blocksize(size_t bs)
    {
        assert(!(bs & (bs - 1)));
        assert(bs <= maxBlockSize);
        assert(bs >= 4);
        blockSize = bs;
        numRegisters = bs >> 2;
        blockSizeInv = 1.f / blockSize;
        registerSizeInv = 1.f / (blockSize >> 2);
    }

  private:
    void updateLine()
    {
        auto cs = SIMD_MM(set1_ps)(current);
        auto dy0 = SIMD_MM(set1_ps)((target - current) * registerSizeInv);
        auto dy = SIMD_MM(mul_ps)(dy0, zeroUpByQuarters);
        for (int i = 0; i < numRegisters; ++i)
        {
            line[i] = SIMD_MM(add_ps)(cs, dy);
            dy = SIMD_MM(add_ps)(dy, dy0);
        }
        current = target;
    }
};
} // namespace sst::basic_blocks::dsp

#endif // SHORTCIRCUITXT_BLOCKINTERPOLATORS_H
