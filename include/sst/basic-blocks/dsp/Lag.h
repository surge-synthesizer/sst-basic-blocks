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

#ifndef SST_BASIC_BLOCKS_DSP_LAG_H
#define SST_BASIC_BLOCKS_DSP_LAG_H

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

  private:
    bool first_run;
    T lp, lpinv;
};
} // namespace sst::basic_blocks::dsp
#endif // SURGE_LAG_H
