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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_QUADRATUREOSCILLATORS_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_QUADRATUREOSCILLATORS_H

#include "sst/basic-blocks/mechanics/block-ops.h"

#include <cmath>

namespace sst::basic_blocks::dsp
{
/**
 * The recurrence oscillator from https://vicanek.de/articles/QuadOsc.pdf
 */
template <typename T = float, int blockSize = 32> struct QuadratureOscillator
{
    T u{1}, v{0}; // u == cos; v == sin
    T k1{0}, k2{0};
    lipol_sse<blockSize> k1Lerp, k2Lerp;
    float k1Block alignas(16)[blockSize];
    float k2Block alignas(16)[blockSize];
    int posInBlock{0};

    inline void setRate(T omega)
    {
        k1 = tan(omega * 0.5);
        k2 = sin(omega);
    }

    inline void step()
    {
        auto w = u - k1 * v;
        v = v + k2 * w;
        u = w - k1 * v;
    }

    // call one of these, followed by blockSize calls of blockStep()
    inline void setRateForBlock(T omega)
    {
        k1Lerp.set_target(tan(omega * 0.5));
        k2Lerp.set_target(sin(omega));
        k1Lerp.store_block(k1Block);
        k2Lerp.store_block(k2Block);
        posInBlock = 0;
    }
    inline void maintainRateForBlock()
    {
        mechanics::set_block<blockSize>(k1Block, k1);
        mechanics::set_block<blockSize>(k2Block, k2);
        posInBlock = 0;
    }

    inline void blockStep()
    {
        k1 = k1Block[posInBlock];
        k2 = k2Block[posInBlock];
        step();
        if (posInBlock < blockSize - 1)
            posInBlock++;
    }

    inline void resetPhase(T p = 0)
    {
        u = 1;
        v = 0;
        k1 = 0;
        k2 = 0;
    }
};

/**
 * The Surge Magic Circle style Oscillator
 */
template <typename T = float> struct SurgeQuadrOsc
{
  public:
    SurgeQuadrOsc()
    {
        r = 0;
        i = -1;
    }

    inline void set_rate(T w)
    {
        dr = cos(w);
        di = sin(w);

        // normalize vector
        double n = 1 / sqrt(r * r + i * i);
        r *= n;
        i *= n;
    }

    // API compatability
    inline void setRate(T w) { set_rate(w); }

    inline void set_phase(T w)
    {
        r = sin(w);
        i = -cos(w);
    }

    inline void process()
    {
        float lr = r, li = i;
        r = dr * lr - di * li;
        i = dr * li + di * lr;
    }

    inline void step() { process(); }

  public:
    T r, i;

  private:
    T dr, di;
};

/**
 * SurgeQuadrOsc with a rate that ramps linearly across a block, rather than stepping at the block
 * boundary. A stepped rate puts sidebands at multiples of the block rate around the oscillator's
 * frequency whenever the rate is modulated, and on anything the oscillator phase modulates.
 *
 * Call set_rate() once per block, then process() blockSize times.
 *
 * Rather than evaluating cos and sin every sample, the per-sample rotation is itself rotated by the
 * per-sample change in rate. That is still a pure rotation, so the amplitude is preserved however
 * large the change in rate is.
 *
 * The ramp is centered on the new rate instead of starting from the prior one, so a block advances
 * the phase by exactly as much as SurgeQuadrOsc would with the rate held constant. A ramp starting
 * from the prior rate lags by half a block, and that lag accumulates into a phase offset
 * proportional to the total change in rate. When the rate doesn't change, the output matches
 * SurgeQuadrOsc's. That match is bit for bit on x86 builds without FMA, but a compiler that
 * contracts to fused multiply-adds (for example GCC with FMA available, such as -march=native on a
 * modern x86 or any aarch64 target) may contract the two differently, and then they differ in the
 * last bits.
 */
template <typename T, int blockSize> struct SurgeQuadrOscRamped
{
  public:
    SurgeQuadrOscRamped()
    {
        r = 0;
        i = -1;
    }

    inline void set_rate(T w)
    {
        // the first block after a phase reset has nothing to ramp from
        if (!primed)
        {
            priorRate = w;
            primed = true;
        }

        T step = (w - priorRate) / blockSize;
        T start = w - step * (T)0.5 * (blockSize - 1);
        priorRate = w;

        dr = cos(start);
        di = sin(start);
        ddr = cos(step);
        ddi = sin(step);

        // normalize vector
        double n = 1 / sqrt(r * r + i * i);
        r *= n;
        i *= n;
    }

    // API compatability
    inline void setRate(T w) { set_rate(w); }

    inline void set_phase(T w)
    {
        r = sin(w);
        i = -cos(w);
        primed = false;
    }

    inline void process()
    {
        T lr = r, li = i;
        r = dr * lr - di * li;
        i = dr * li + di * lr;

        T ldr = dr, ldi = di;
        dr = ddr * ldr - ddi * ldi;
        di = ddr * ldi + ddi * ldr;
    }

    inline void step() { process(); }

  public:
    T r, i;

  private:
    T dr{1}, di{0}, ddr{1}, ddi{0}, priorRate{0};
    bool primed{false};
};
} // namespace sst::basic_blocks::dsp

#endif // SHORTCIRCUITXT_QUADRATUREOSCILLATORS_H
