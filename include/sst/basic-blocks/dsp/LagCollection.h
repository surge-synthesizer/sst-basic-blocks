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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_LAGCOLLECTION_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_LAGCOLLECTION_H

#if SST_CPPUTILS_UNAVAILABLE
#else

#include <cassert>
#include "sst/cpputils/active_set_overlay.h"
#include "Lag.h"

namespace sst::basic_blocks::dsp
{
/**
 * LagCollection is a bucket of laggers which act as a group and allow you
 * to easily manage the active set. Basically use it with <128> to smooth
 * midi ccs and stuff like that
 *
 * @tparam N how many laggers to have
 */
template <int N> struct LagCollection
{
    struct Lagger : sst::cpputils::active_set_overlay<Lagger>::participant
    {
        void process()
        {
            lag.process();
            apply();
        }

        void snapToTarget()
        {
            lag.snapToTarget();
            apply();
        }
        void apply()
        {
            if (onto)
            {
                *onto = lag.v;
            }
        }
        float *onto{nullptr};
        LinearLag<float, true> lag;
        int index;
    };

    std::array<Lagger, N> lags;
    sst::cpputils::active_set_overlay<Lagger> activeSet;

    LagCollection()
    {
        for (int i = 0; i < N; ++i)
            lags[i].index = i;
    }

    void setRateInMilliseconds(double rate, double sampleRate, double blockSizeInv)
    {
        for (auto &l : lags)
            l.lag.setRateInMilliseconds(1000.0 * rate / 48000.0, sampleRate, blockSizeInv);
    }

    void setTarget(size_t index, float target, float *onto)
    {
        assert(index < N);
        lags[index].lag.setTarget(target);
        lags[index].onto = onto;
        activeSet.addToActive(lags[index]);
    }

    void processAll()
    {
        for (auto &lag : activeSet)
        {
            lag.process();
            if (!lag.lag.isActive())
                activeSet.removeFromActive(lag);
        }
    }

    void snapAllActiveToTarget()
    {
        for (auto &lag : activeSet)
        {
            lag.snapToTarget();
        }
        while (activeSet.begin() != activeSet.end())
            activeSet.removeFromActive(*activeSet.begin());
    }
};
} // namespace sst::basic_blocks::dsp
#endif
#endif // LAGCOLLECTION_H
