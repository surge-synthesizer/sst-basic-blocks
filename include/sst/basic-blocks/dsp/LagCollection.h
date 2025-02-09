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
    struct Lagger
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
        Lagger *next{nullptr}, *prev{nullptr};
    };

    std::array<Lagger, N> lags;
    Lagger *activeHead{nullptr};

    void setRateInMilliseconds(double rate, double sampleRate, double blockSizeInv)
    {
        for (auto &l : lags)
            l.lag.setRateInMilliseconds(1000.0 * 64.0 / 48000.0, sampleRate, blockSizeInv);
    }

    void setTarget(size_t index, float target, float *onto)
    {
        assert(index < N);
        lags[index].lag.setTarget(target);
        lags[index].onto = onto;

        if (lags[index].next == nullptr && lags[index].prev == nullptr)
        {
            lags[index].next = activeHead;
            activeHead = &lags[index];
        }
    }

    void processAll()
    {
        auto curr = activeHead;
        while (curr)
        {
            curr->process();
            if (!curr->lag.isActive())
            {
                if (curr->next)
                    curr->next->prev = curr->prev;
                if (curr->prev)
                    curr->prev->next = curr->next;
                if (curr == activeHead)
                    activeHead = curr->next;

                auto nv = curr->next;
                curr->next = nullptr;
                curr->prev = nullptr;
                curr->onto = nullptr;
                curr = nv;
            }
            else
            {
                curr = curr->next;
            }
        }
    }

    void snapAllActiveToTarget()
    {
        auto curr = activeHead;
        while (curr)
        {
            curr->snapToTarget();
            auto nv = curr->next;
            curr->next = nullptr;
            curr->prev = nullptr;
            curr = nv;
        }
        activeHead = nullptr;
    }
};
} // namespace sst::basic_blocks::dsp
#endif // LAGCOLLECTION_H
