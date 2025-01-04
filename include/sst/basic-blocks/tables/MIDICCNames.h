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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_MIDICCNAMES_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_MIDICCNAMES_H

#include <string>

namespace sst::basic_blocks::tables
{
static inline std::string MIDI1CCLongName(int cc)
{
    if (cc < 0 || cc > 128)
        return "<error>";

    const std::string midicc_names[128] = {"Bank Select MSB",
                                           "Modulation Wheel MSB",
                                           "Breath Controller MSB",
                                           "Control 3 MSB",
                                           "Foot Controller MSB",
                                           "Portamento Time MSB",
                                           "Data Entry MSB",
                                           "Volume MSB",
                                           "Balance MSB",
                                           "Control 9 MSB",
                                           "Pan MSB",
                                           "Expression MSB",
                                           "Effect #1 MSB",
                                           "Effect #2 MSB",
                                           "Control 14 MSB",
                                           "Control 15 MSB",
                                           "General Purpose Controller #1 MSB",
                                           "General Purpose Controller #2 MSB",
                                           "General Purpose Controller #3 MSB",
                                           "General Purpose Controller #4 MSB",
                                           "Control 20 MSB",
                                           "Control 21 MSB",
                                           "Control 22 MSB",
                                           "Control 23 MSB",
                                           "Control 24 MSB",
                                           "Control 25 MSB",
                                           "Control 26 MSB",
                                           "Control 27 MSB",
                                           "Control 28 MSB",
                                           "Control 29 MSB",
                                           "Control 30 MSB",
                                           "Control 31 MSB",
                                           "Bank Select LSB",
                                           "Modulation Wheel LSB",
                                           "Breath Controller LSB",
                                           "Control 3 LSB",
                                           "Foot Controller LSB",
                                           "Portamento Time LSB",
                                           "Data Entry LSB",
                                           "Volume LSB",
                                           "Balance LSB",
                                           "Control 9 LSB",
                                           "Pan LSB",
                                           "Expression LSB",
                                           "Effect #1 LSB",
                                           "Effect #2 LSB",
                                           "Control 14 LSB",
                                           "Control 15 LSB",
                                           "General Purpose Controller #1 LSB",
                                           "General Purpose Controller #2 LSB",
                                           "General Purpose Controller #3 LSB",
                                           "General Purpose Controller #4 LSB",
                                           "Control 20 LSB",
                                           "Control 21 LSB",
                                           "Control 22 LSB",
                                           "Control 23 LSB",
                                           "Control 24 LSB",
                                           "Control 25 LSB",
                                           "Control 26 LSB",
                                           "Control 27 LSB",
                                           "Control 28 LSB",
                                           "Control 29 LSB",
                                           "Control 30 LSB",
                                           "Control 31 LSB",
                                           "Sustain Pedal",
                                           "Portamento Pedal",
                                           "Sostenuto Pedal",
                                           "Soft Pedal",
                                           "Legato Pedal",
                                           "Hold Pedal",
                                           "Sound Control #1 Sound Variation",
                                           "Sound Control #2 Timbre",
                                           "Sound Control #3 Release Time",
                                           "Sound Control #4 Attack Time",
                                           "Sound Control #5 Brightness / MPE Timbre",
                                           "Sound Control #6 Decay Time",
                                           "Sound Control #7 Vibrato Rate",
                                           "Sound Control #8 Vibrato Depth",
                                           "Sound Control #9 Vibrato Delay",
                                           "Sound Control #10 Control 79",
                                           "General Purpose Controller #5",
                                           "General Purpose Controller #6",
                                           "General Purpose Controller #7",
                                           "General Purpose Controller #8",
                                           "Portamento Control",
                                           "Control 85",
                                           "Control 86",
                                           "Control 87",
                                           "High Resolution Velocity Prefix",
                                           "Control 89",
                                           "Control 90",
                                           "Reverb Send Level",
                                           "Tremolo Depth",
                                           "Chorus Send Level",
                                           "Celeste Depth",
                                           "Phaser Depth",
                                           "Data Increment",
                                           "Data Decrement",
                                           "NRPN LSB",
                                           "NRPN MSB",
                                           "RPN LSB",
                                           "RPN MLSB",
                                           "Control 102",
                                           "Control 103",
                                           "Control 104",
                                           "Control 105",
                                           "Control 106",
                                           "Control 107",
                                           "Control 108",
                                           "Control 109",
                                           "Control 110",
                                           "Control 111",
                                           "Control 112",
                                           "Control 113",
                                           "Control 114",
                                           "Control 115",
                                           "Control 116",
                                           "Control 117",
                                           "Control 118",
                                           "Control 119",
                                           "Control 120 All Sound Off",
                                           "Control 121 Reset All Controllers",
                                           "Control 122 Local Control On/Off",
                                           "Control 123 All Notes Off",
                                           "Control 124 Omni Mode Off",
                                           "Control 125 Omni Mode On",
                                           "Control 126 Mono Mode Off",
                                           "Control 127 Mono Mode On"};
    return midicc_names[cc];
}

/*
 * Abbreviated names suitable for augmenting a menu which sohows midi CCs.
 * Will return blank for things like "High Resolution Velocity Prefix"
 */
static inline std::string MIDI1CCVeryShortName(int cc)
{
    if (cc < 0 || cc > 128)
        return "<error>";

    const std::string midicc_names[128] = {
        "",           // "Bank Select MSB",
        "Mod Wheel",  //"Modulation Wheel MSB",
        "Breath",     //"Breath Controller MSB",
        "",           // "Control 3 MSB",
        "Foot",       //"Foot Controller MSB",
        "Porta Time", //"Portamento Time MSB",
        "",           //"Data Entry MSB",
        "Volume",     //"Volume MSB",
        "Balance",    //"Balance MSB",
        "",           //"Control 9 MSB",
        "Pan",        //"Pan MSB",
        "Expression", //"Expression MSB",
        "",           // Effect #1 MSB",
        "",           // "Effect #2 MSB",
        "",           // "Control 14 MSB",
        "",           // "Control 15 MSB",
        "",           // "General Purpose Controller #1 MSB",
        "",           // "General Purpose Controller #2 MSB",
        "",           // "General Purpose Controller #3 MSB",
        "",           // "General Purpose Controller #4 MSB",
        "",           // "Control 20 MSB",
        "",           // "Control 21 MSB",
        "",           // "Control 22 MSB",
        "",           // "Control 23 MSB",
        "",           // "Control 24 MSB",
        "",           // "Control 25 MSB",
        "",           // "Control 26 MSB",
        "",           // "Control 27 MSB",
        "",           // "Control 28 MSB",
        "",           // "Control 29 MSB",
        "",           // "Control 30 MSB",
        "",           // "Control 31 MSB",
        "",           // "Bank Select LSB",
        "",           // "Modulation Wheel LSB",
        "",           // "Breath Controller LSB",
        "",           // "Control 3 LSB",
        "",           // "Foot Controller LSB",
        "",           // "Portamento Time LSB",
        "",           // "Data Entry LSB",
        "",           // "Volume LSB",
        "",           // "Balance LSB",
        "",           // "Control 9 LSB",
        "",           // "Pan LSB",
        "",           // "Expression LSB",
        "",           // "Effect #1 LSB",
        "",           // "Effect #2 LSB",
        "",           // "Control 14 LSB",
        "",           // "Control 15 LSB",
        "",           // "General Purpose Controller #1 LSB",
        "",           // "General Purpose Controller #2 LSB",
        "",           // "General Purpose Controller #3 LSB",
        "",           // "General Purpose Controller #4 LSB",
        "",           // "Control 20 LSB",
        "",           // "Control 21 LSB",
        "",           // "Control 22 LSB",
        "",           // "Control 23 LSB",
        "",           // "Control 24 LSB",
        "",           // "Control 25 LSB",
        "",           // "Control 26 LSB",
        "",           // "Control 27 LSB",
        "",           // "Control 28 LSB",
        "",           // "Control 29 LSB",
        "",           // "Control 30 LSB",
        "",           // "Control 31 LSB",
        "Sustain Pedal",
        "Portamento Pedal",
        "Sostenuto Pedal",
        "Soft Pedal",
        "Legato Pedal",
        "Hold Pedal",
        "",              // "Sound Control #1 Sound Variation",
        "",              // "Sound Control #2 Timbre",
        "",              // "Sound Control #3 Release Time",
        "",              // "Sound Control #4 Attack Time",
        "",              // "Sound Control #5 Brightness / MPE Timbre",
        "",              // "Sound Control #6 Decay Time",
        "",              // "Sound Control #7 Vibrato Rate",
        "",              // "Sound Control #8 Vibrato Depth",
        "",              // "Sound Control #9 Vibrato Delay",
        "",              // "Sound Control #10 Control 79",
        "",              // "General Purpose Controller #5",
        "",              // "General Purpose Controller #6",
        "",              // "General Purpose Controller #7",
        "",              // "General Purpose Controller #8",
        "",              // "Portamento Control",
        "",              // "Control 85",
        "",              // "Control 86",
        "",              // "Control 87",
        "",              // "High Resolution Velocity Prefix",
        "",              // "Control 89",
        "",              // "Control 90",
        "",              // "Reverb Send Level",
        "",              // "Tremolo Depth",
        "",              // "Chorus Send Level",
        "",              // "Celeste Depth",
        "",              // "Phaser Depth",
        "",              // "Data Increment",
        "",              // "Data Decrement",
        "",              // "NRPN LSB",
        "",              // "NRPN MSB",
        "",              // "RPN LSB",
        "",              // "RPN MLSB",
        "",              // "Control 102",
        "",              // "Control 103",
        "",              // "Control 104",
        "",              // "Control 105",
        "",              // "Control 106",
        "",              // "Control 107",
        "",              // "Control 108",
        "",              // "Control 109",
        "",              // "Control 110",
        "",              // "Control 111",
        "",              // "Control 112",
        "",              // "Control 113",
        "",              // "Control 114",
        "",              // "Control 115",
        "",              // "Control 116",
        "",              // "Control 117",
        "",              // "Control 118",
        "",              // "Control 119",
        "All Sound Off", // "Control 120 All Sound Off",
        "",              // "Control 121 Reset All Controllers",
        "",              // "Control 122 Local Control On/Off",
        "All Notes Off", //"Control 123 All Notes Off",
        "",              // "Control 124 Omni Mode Off",
        "",              // "Control 125 Omni Mode On",
        "",              // "Control 126 Mono Mode Off",
        "",              // "Control 127 Mono Mode On"
    };
    return midicc_names[cc];
}
} // namespace sst::basic_blocks::tables
#endif // MIDI1CCNAMES_H
