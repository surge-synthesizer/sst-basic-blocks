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

#include <cmath>

namespace sst::basic_blocks::dsp
{
/**
 * The recurrence oscillator from https://vicanek.de/articles/QuadOsc.pdf
 */
template <typename T = float> struct QuadratureOscillator
{
    T u{1}, v{0}; // u == cos; v == sin
    T k1{0}, k2{0};

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
} // namespace sst::basic_blocks::dsp

#endif // SHORTCIRCUITXT_QUADRATUREOSCILLATORS_H
