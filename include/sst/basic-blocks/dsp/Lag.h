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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_LAG_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_LAG_H

namespace sst::basic_blocks::dsp
{
template <class T, bool first_run_checks = true> struct SurgeLag
{
  public:
    SurgeLag(T lp) { setRate(lp); }

    SurgeLag() { setRate(0.004); }

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
    inline T getValue() { return v; }

    inline void process() { v = v * lpinv + target_v * lp; }

    T v{0};
    T target_v{0};

    bool first_run{true};

  private:
    T lp{0}, lpinv{0};
};

/*
 * Linearly lag a float value onto a destination. Takes a pointer to the destination
 * and the target. Handles restatements properly etc...
 *
 * Intended uses case is for a per-block processor while bound to a UI element
 *
 * If target is reached, does nothing other than a single branch / return
 */
struct UIComponentLagHandler
{
    float *destination{nullptr};
    float targetValue{0.f};
    float value{0.f};
    float dTarget{0.f}, dTargetScale{0.05f};
    bool active{false};

    void setRate(float rateInHz, uint16_t blockSize, float sampleRate)
    {
        int blocks = (int)std::round(sampleRate / rateInHz / blockSize);
        dTargetScale = 1.f / blocks;
    }

    void setNewDestination(float *d, float toTarget)
    {
        if (active && d == destination)
        {
            // restating an active target
            setTarget(toTarget);
        }
        else
        {
            if (active)
            {
                // We are still lagging the prior. Rare case. Just speed it up.
                *destination = targetValue;
            }
            value = *d;
            destination = d;
            setTarget(toTarget);
        }
    }

    void setTarget(float t)
    {
        targetValue = t;
        dTarget = (targetValue - value) * dTargetScale;
        active = true;
    }

    void process()
    {
        if (!active)
            return;

        value += dTarget;
        if (std::fabs(value - targetValue) < std::fabs(dTarget))
        {
            value = targetValue;
            active = false;
        }
        *destination = value;
    }

    void instantlySnap()
    {
        *destination = targetValue;
        active = false;
    }
};
} // namespace sst::basic_blocks::dsp
#endif // SURGE_LAG_H
