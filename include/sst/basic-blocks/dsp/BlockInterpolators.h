/*
 * sst-basic-blocks - a Surge Synth Team product
 *
 * Provides basic tools and blocks for use on the audio thread in
 * synthesis, including basic DSP and modulation functions
 *
 * Copyright 2019 - 2023, Various authors, as described in the github
 * transaction log.
 *
 * sst-basic-blocks is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source for sst-basic-blocks is available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef SST_BASIC_BLOCKS_DSP_BLOCK_INTERPOLATORS_H
#define SST_BASIC_BLOCKS_DSP_BLOCK_INTERPOLATORS_H

namespace sst::basic_blocks::dsp
{
template <class T, int blockSize, bool first_run_checks = true> struct lipol
{
  public:
    static constexpr T bs_inv{(T)1 / (T)blockSize};
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
    inline T getTargetValue() { return new_v; }
    inline void process() { v += dv; }
    T v;

  private:
    T new_v{0};
    T dv{0};
    bool first_run{true};
};

template <int blockSize, bool first_run_checks = true> struct lipol_sse
{
    lipol_sse()
    {
        float zbq alignas(16)[4]{0.f, 0.25f, 0.5f, 0.75f};
        zeroUpByQuarters = _mm_load_ps(zbq);
        one = _mm_set1_ps(1.f);
    }
    void set_target(float f)
    {
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

    void set_target_instant(float f)
    {
        target = f;
        current = f;
        updateLine();
    }

    /*
     * Out = in * linearly-interpolated-target
     */
    void multiply_block_to(float *__restrict in, float *__restrict out)
    {
        for (int i = 0; i < numRegisters; ++i)
        {
            auto iv = _mm_load_ps(in + (i << 2));
            auto ov = _mm_mul_ps(iv, line[i]);
            _mm_store_ps(out + (i << 2), ov);
        }
    }
    void multiply_2_blocks_to(float *__restrict inL, float *__restrict inR, float *__restrict outL,
                              float *__restrict outR)
    {
        multiply_block_to(inL, outL);
        multiply_block_to(inR, outR);
    }

    /*
     * out = a * (1-t) + b * t
     */
    void fade_blocks(float *__restrict inA, float *__restrict inB, float *__restrict out)
    {
        for (int i = 0; i < numRegisters; ++i)
        {
            auto a = _mm_load_ps(inA + (i << 2));
            auto b = _mm_load_ps(inB + (i << 2));
            auto sa = _mm_mul_ps(a, _mm_sub_ps(one, line[i]));
            auto sb = _mm_mul_ps(b, line[i]);
            auto r = _mm_add_ps(sa, sb);
            _mm_store_ps(out + (i << 2), r);
        }
    }

    void store_block(float *__restrict out)
    {
        for (int i = 0; i < numRegisters; ++i)
        {
            _mm_store_ps(out + (i << 2), line[i]);
        }
    }

  private:
    void updateLine()
    {
        auto cs = _mm_set1_ps(current);
        auto dy0 = _mm_set1_ps((target - current) * registerSizeInv);
        auto dy = _mm_mul_ps(dy0, zeroUpByQuarters);
        for (int i = 0; i < numRegisters; ++i)
        {
            line[i] = _mm_add_ps(cs, dy);
            dy = _mm_add_ps(dy, dy0);
        }
        current = target;
    }

    static constexpr int numRegisters{blockSize >> 2};
    static constexpr float blockSizeInv{1.f / blockSize};
    static constexpr float registerSizeInv{1.f / (blockSize >> 2)};
    __m128 line[numRegisters];
    __m128 zeroUpByQuarters;
    __m128 one;
    float target{0.f}, current{0.f};
    bool first_run{true};
};
} // namespace sst::basic_blocks::dsp

#endif // SHORTCIRCUITXT_BLOCKINTERPOLATORS_H
