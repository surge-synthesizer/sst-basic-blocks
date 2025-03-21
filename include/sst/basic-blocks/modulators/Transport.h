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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_TRANSPORT_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_TRANSPORT_H

#include <cstdint>

namespace sst::basic_blocks::modulators
{
struct Transport
{
    struct Signature
    {
        uint16_t numerator{4};
        uint16_t denominator{4};
    } signature{};

    enum Status
    {
        STOPPED = 0,
        PLAYING = 1 << 1,
        RECORDING = 1 << 2,
        LOOPING = 1 << 3
    };

    uint32_t status{STOPPED};

    double tempo{120};

    /*
     * hostTimeInBeats is the time from the host. When playing is
     * stopped it will freeze. timeInBeats is an internal counter which
     * will synch with the host when playing but will continue to
     * advcance at tempo when stopped. Use timeInBeats for things like
     * ppq synced modulators, but hostTimeInBeats for things like
     * display.
     */
    double hostTimeInBeats{0};
    double timeInBeats{0};
    double lastBarStartInBeats{0};
};
} // namespace sst::basic_blocks::modulators
#endif // TRANSPORT_H
