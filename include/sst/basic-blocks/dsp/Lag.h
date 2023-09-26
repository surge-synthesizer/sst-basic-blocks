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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_LAG_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_LAG_H

namespace sst::basic_blocks::dsp
{
template <class T, bool first_run_checks = true> struct SurgeLag
{
  public:
    SurgeLag(T lp)
    {
        this->lp = lp;
        lpinv = 1 - lp;
        v = 0;
        target_v = 0;

        if (first_run_checks)
        {
            first_run = true;
        }
    }

    SurgeLag()
    {
        lp = 0.004;
        lpinv = 1 - lp;
        v = 0;
        target_v = 0;

        if (first_run_checks)
        {
            first_run = true;
        }
    }

    void setRate(T lp)
    {
        this->lp = lp;
        lpinv = 1 - lp;
    }

    inline void newValue(T f)
    {
        target_v = f;

        if (first_run_checks && first_run)
        {
            v = target_v;
            first_run = false;
        }
    }

    inline void startValue(T f)
    {
        target_v = f;
        v = f;

        if (first_run_checks && first_run)
        {
            first_run = false;
        }
    }

    inline void instantize() { v = target_v; }

    inline T getTargetValue() { return target_v; }

    inline void process() { v = v * lpinv + target_v * lp; }

    T v;
    T target_v;

    bool first_run;

  private:
    T lp, lpinv;
};
} // namespace sst::basic_blocks::dsp
#endif // SURGE_LAG_H
