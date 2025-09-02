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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_TRANSPORTCLAPADAPTER_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_TRANSPORTCLAPADAPTER_H

#include "Transport.h"

/*
 * This file is purposefully fragile w.r.t clap headers
 * which are not a dependency of basic blocks. If you arent
 * using transport class with clap, just don't include this
 * header
 */

#ifdef CLAP_VERSION_MAJOR
namespace sst::basic_blocks::modulators
{
void fromClapTransport(Transport &that, const clap_event_transport_t *t)
{
    if (t)
    {
        that.tempo = t->tempo;

        // isRecording should always imply isPlaying but better safe than sorry
        if (t->flags & CLAP_TRANSPORT_IS_PLAYING || t->flags & CLAP_TRANSPORT_IS_RECORDING)
        {
            that.hostTimeInBeats = 1.0 * t->song_pos_beats / CLAP_BEATTIME_FACTOR;
            that.lastBarStartInBeats = 1.0 * t->bar_start / CLAP_BEATTIME_FACTOR;
            that.timeInBeats = that.hostTimeInBeats;
        }

        that.status = Transport::STOPPED;
        if (t->flags & CLAP_TRANSPORT_IS_PLAYING)
            that.status |= Transport::PLAYING;
        if (t->flags & CLAP_TRANSPORT_IS_RECORDING)
            that.status |= Transport::RECORDING;
        if (t->flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE)
            that.status |= Transport::LOOPING;

        that.signature.numerator = t->tsig_num;
        that.signature.denominator = t->tsig_denom;
    }
    else
    {
        that.tempo = 120;
        that.signature.numerator = 4;
        that.signature.denominator = 4;
        that.status = Transport::STOPPED;
    }
}
} // namespace sst::basic_blocks::modulators
#endif

#endif // TRANSPORTCLAPADAPTER_H
