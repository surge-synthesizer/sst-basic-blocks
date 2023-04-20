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

#ifndef SST_BASICBLOCKS_DSP_QUADRATUREOSCILLATORS_H
#define SST_BASICBLOCKS_DSP_QUADRATUREOSCILLATORS_H

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
