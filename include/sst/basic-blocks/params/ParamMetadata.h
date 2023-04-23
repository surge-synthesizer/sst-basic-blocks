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
#ifndef INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H
#define INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H

#include <string>

namespace sst::basic_blocks::params
{
struct ParamMetaData
{
    ParamMetaData() = default;

    enum Type
    {
        FLOAT,
        INT,
        BOOL
    } type{FLOAT};

    std::string name;

    float minVal{0.f}, maxVal{1.f}, defaultVal{0.f};
    bool canExtend{false}, canDeform{false}, canAbsolute{false}, canTemposync{false};

    ParamMetaData &withType(Type t) {
        type = t;
        return *this;
    }
    ParamMetaData &withName(const std::string t) {
        name = t;
        return *this;
    }
    ParamMetaData &withRange(float mn, float mx) {
        minVal = mn;
        maxVal = mx;
        defaultVal = std::clamp(defaultVal, minVal, maxVal);
        return *this;
    }
    ParamMetaData &withDefault(float t) {
        defaultVal = t;
        return *this;
    }
    ParamMetaData &extendable(bool b=true) {
        canExtend = b;
        return *this;
    }
    ParamMetaData &deformable(bool b=true) {
        canDeform = b;
        return *this;
    }
    ParamMetaData &absolutable(bool b=true) {
        canAbsolute = b;
        return *this;
    }
    ParamMetaData &temposyncable(bool b=true) {
        canTemposync = b;
        return *this;
    }

    ParamMetaData &asPercent()
    {
        return withRange(0.f, 1.f).withDefault(0.f).withType(FLOAT);
    }
    ParamMetaData &asPercentBipolar()
    {
        return withRange(-1.f, 1.f).withDefault(0.f).withType(FLOAT);
    }
    ParamMetaData &asDecibelNarrow()
    {
        return withRange(-24.f, 24.f).withDefault(0.f).withType(FLOAT);
    }
    ParamMetaData &asDecibel()
    {
        return withRange(-48.f, 48.f).withDefault(0.f).withType(FLOAT);
    }
    ParamMetaData &asMIDIPitch()
    {
        return withType(FLOAT).withRange(0.f, 127.f).withDefault(60.f);
    }
    ParamMetaData &asEnvelopeTime()
    {
        return withType(FLOAT).withRange(-8.f, 5.f).withDefault(-1.f);
    }
    ParamMetaData &asAudibleFrequency()
    {
        return withType(FLOAT).withRange(-60, 70).withDefault(0);
    }


};
}

#endif