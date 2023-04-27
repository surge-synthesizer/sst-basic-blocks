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

/*
 * Please note this class is still a work in active development. There's an enormous amount
 * to do including
 *
 * - string conversion for absolute, extend, etc...
 * - custom string functions
 * - midi notes get note name typeins
 * - alternate displays
 * - and much much more
 *
 * Right now it just has the features paul needed to port flanger and reverb1 to sst-effects
 * so still expect change to be coming.
 *
 * those are all required before this can be a complete sub-in for surge's Parameter.cpp
 * formatting and description. If you are using this right now please hang on our discord
 * or github to discuss! Or wait until there's a commit removing this message and this is
 * used as a dependency of a production release of Surge or ShortCircuit VSTs
 */

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <unordered_map>

#include <fmt/core.h>

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

    bool supportsStringConversion{false};

    /*
     * What is the primary string representation of this value
     */
    std::optional<std::string> valueToString(float val, bool highPrecision = false) const;

    /*
     * Some parameters have a secondary representation. For instance 441.2hz could also be ~A4.
     * If this parameter supports that it will return a string value from this API. Surge uses
     * this in the left side of the tooltip.
     */
    std::optional<std::string> valueToAlternateString(float val) const;

    /*
     * Convert a value to a string; if the optional is empty populate the error message.
     */
    std::optional<float> valueFromString(const std::string_view &, std::string &errMsg) const;

    /*
     * Distances to String conversions are more peculiar, especially with non-linear ranges.
     * The parameter metadata assumes all distances are represented in a [-1,1] value on the
     * range of the parameter, and then can create four strings for a given distance:
     * - value up/down (to string of val +/- modulation)
     * - distance up (the string of the distance of appying the modulation up down)
     * To calculate these, modulation needs to be expressed as a natural base value
     * and a percentage modulation depth.
     */
    struct ModulationDisplay
    {
        // value is with-units value suitable to seed a typein. Like "4.3 semitones"
        std::string value;
        // Summary is a brief description suitable for a menu like "+/- 13.2%"
        std::string summary;

        // baseValue, valUp/Dn and changeUp/Dn are no unit indications of change in each direction.
        std::string baseValue, valUp, valDown, changeUp, changeDown;
    };
    std::optional<ModulationDisplay> modulationNaturalToString(float naturalBaseVal,
                                                               float modulationNatural,
                                                               bool isBipolar,
                                                               bool highPrecision = false) const;
    std::optional<float> modulationNaturalFromString(const std::string_view &deltaNatural,
                                                     float naturalBaseVal,
                                                     std::string &errMsg) const;

    enum DisplayScale
    {
        LINEAR,
        A_TWO_TO_THE_B,
        DECIBEL,
        UNORDERED_MAP,
        USER_PROVIDED
    } displayScale{LINEAR};

  protected:
    std::string unit{};
    std::string customMinDisplay{}, customMaxDisplay{};
    std::unordered_map<int, std::string> discreteValues;
    int decimalPlaces{2};
    float svA{0.f}, svB{0.f}, svC{0.f}, svD{0.f}; // for various functional forms

  public:
    float naturalToNormalized01(float naturalValue) const
    {
        float v = 0;
        switch (type)
        {
        case FLOAT:
            assert(maxVal != minVal);
            v = (naturalValue - minVal) / (maxVal - minVal);
            break;
        case INT:
            assert(maxVal != minVal);
            // This is the surge conversion. Do we want to keep it?
            v = 0.005 + 0.99 * (naturalValue - minVal) / (maxVal - minVal);
            break;
        case BOOL:
            assert(maxVal == 1 && minVal == 0);
            v = (naturalValue > 0.5 ? 1.f : 0.f);
            break;
        }
        return std::clamp(v, 0.f, 1.f);
    }
    float normalized01ToNatural(float normalizedValue) const
    {
        assert(normalizedValue >= 0.f && normalizedValue <= 1.f);
        assert(maxVal != minVal);
        normalizedValue = std::clamp(normalizedValue, 0.f, 1.f);
        switch (type)
        {
        case FLOAT:
            return normalizedValue * (maxVal - minVal) + minVal;
        case INT:
        {
            // again the surge conversion
            return (int)((1 / 0.99) * (normalizedValue - 0.005) * (maxVal - minVal) + 0.5) + minVal;
        }
        case BOOL:
            assert(maxVal == 1 && minVal == 0);
            return normalizedValue > 0.5 ? maxVal : minVal;
        }
        // quiet gcc
        return 0.f;
    }

    ParamMetaData &withType(Type t)
    {
        type = t;
        return *this;
    }
    ParamMetaData &withName(const std::string t)
    {
        name = t;
        return *this;
    }
    ParamMetaData &withRange(float mn, float mx)
    {
        minVal = mn;
        maxVal = mx;
        defaultVal = std::clamp(defaultVal, minVal, maxVal);
        return *this;
    }
    ParamMetaData &withDefault(float t)
    {
        defaultVal = t;
        return *this;
    }
    ParamMetaData &extendable(bool b = true)
    {
        canExtend = b;
        return *this;
    }
    ParamMetaData &deformable(bool b = true)
    {
        canDeform = b;
        return *this;
    }
    ParamMetaData &absolutable(bool b = true)
    {
        canAbsolute = b;
        return *this;
    }
    ParamMetaData &temposyncable(bool b = true)
    {
        canTemposync = b;
        return *this;
    }

    ParamMetaData &withATwoToTheBFormatting(float A, float B, const std::string_view &units)
    {
        svA = A;
        svB = B;
        unit = units;
        displayScale = A_TWO_TO_THE_B;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withSemitoneZeroAt400Formatting()
    {
        return withATwoToTheBFormatting(440, 1.0 / 12.0, "Hz");
    }
    ParamMetaData &withLog2SecondsFormatting() { return withATwoToTheBFormatting(1, 1, "s"); }

    ParamMetaData &withLinearScaleFormatting(std::string units, float scale = 1.f)
    {
        svA = scale;
        unit = units;
        displayScale = LINEAR;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withUnorderedMapFormatting(const std::unordered_map<int, std::string> &map)
    {
        discreteValues = map;
        displayScale = UNORDERED_MAP;
        supportsStringConversion = true;
        return *this;
    }

    ParamMetaData &withDecimalPlaces(int d)
    {
        decimalPlaces = d;
        return *this;
    }

    ParamMetaData &asPercent()
    {
        return withRange(0.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withDecimalPlaces(2);
    }

    ParamMetaData &asPercentBipolar()
    {
        return withRange(-1.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withDecimalPlaces(2);
    }
    ParamMetaData &asDecibelNarrow()
    {
        return withRange(-24.f, 24.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("dB");
    }
    ParamMetaData &asDecibel()
    {
        return withRange(-48.f, 48.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("dB");
    }
    ParamMetaData &asMIDIPitch()
    {
        return withType(FLOAT)
            .withRange(0.f, 127.f)
            .withDefault(60.f)
            .withLinearScaleFormatting("semitones");
    }
    ParamMetaData &asEnvelopeTime()
    {
        return withType(FLOAT)
            .withRange(-8.f, 5.f)
            .withDefault(-1.f)
            .temposyncable()
            .withATwoToTheBFormatting(1, 1, "s");
    }
    ParamMetaData &asAudibleFrequency()
    {
        return withType(FLOAT).withRange(-60, 70).withDefault(0).withSemitoneZeroAt400Formatting();
    }
};

/*
 * Implementation below here
 */
inline std::optional<std::string> ParamMetaData::valueToString(float val, bool highPrecision) const
{
    if (type == BOOL)
    {
        if (val < 0)
            return customMinDisplay.empty() ? "Off" : customMinDisplay;
        return customMaxDisplay.empty() ? "On" : customMaxDisplay;
    }

    if (type == INT)
    {
        auto iv = (int)std::round(val);
        if (displayScale == UNORDERED_MAP)
        {
            if (discreteValues.find(iv) != discreteValues.end())
                return discreteValues.at(iv);
            return {};
        }
        return {};
    }

    // float cases
    switch (displayScale)
    {
    case LINEAR:
        if (val == minVal && !customMinDisplay.empty())
        {
            return customMinDisplay;
        }
        if (val == maxVal && !customMaxDisplay.empty())
        {
            return customMinDisplay;
        }

        return fmt::format("{:.{}f} {:s}", svA * val,
                           (highPrecision ? (decimalPlaces + 4) : decimalPlaces), unit);
        break;
    case A_TWO_TO_THE_B:
        if (val == minVal && !customMinDisplay.empty())
        {
            return customMinDisplay;
        }
        if (val == maxVal && !customMaxDisplay.empty())
        {
            return customMinDisplay;
        }

        return fmt::format("{:.{}f} {:s}", svA * pow(2.0, svB * val),
                           (highPrecision ? (decimalPlaces + 4) : decimalPlaces), unit);
        break;
    default:
        break;
    }
    return {};
}

inline std::optional<float> ParamMetaData::valueFromString(const std::string_view &v,
                                                           std::string &errMsg) const
{
    if (type == BOOL)
        return {};
    if (type == INT)
        return {};

    if (!customMinDisplay.empty() && v == customMinDisplay)
        return minVal;

    if (!customMaxDisplay.empty() && v == customMaxDisplay)
        return maxVal;

    auto rangeMsg = [this]() {
        std::string em;
        auto nv = valueToString(minVal);
        auto xv = valueToString(maxVal);
        if (nv.has_value() && xv.has_value())
            em = fmt::format("{} < val < {}", *nv, *xv);
        else
            em = fmt::format("Invalid input");
        return em;
    };
    switch (displayScale)
    {
    case LINEAR:
    {
        try
        {
            auto r = std::stof(std::string(v));
            assert(svA != 0);
            r = r / svA;
            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return {};
            }

            return r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return {};
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto r = std::stof(std::string(v));
            assert(svA != 0);
            assert(svB != 0);
            if (r < 0)
            {
                errMsg = rangeMsg();
                return {};
            }
            r = log2(r / svA) / svB;
            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return {};
            }

            return r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return {};
        }
    }
    break;
    default:
        break;
    }
    return {};
}

inline std::optional<std::string> ParamMetaData::valueToAlternateString(float val) const
{
    return {};
}

inline std::optional<ParamMetaData::ModulationDisplay>
ParamMetaData::modulationNaturalToString(float naturalBaseVal, float modulationNatural,
                                         bool isBipolar, bool highPrecision) const
{
    if (type != FLOAT)
        return {};
    ModulationDisplay result;

    switch (displayScale)
    {
    case LINEAR:
    {
        assert(abs(modulationNatural) <= maxVal - minVal);
        // OK this is super easy. It's just linear!
        auto du = modulationNatural;
        auto dd = -modulationNatural;

        auto dp = (highPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f} {}", svA * du, dp, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary = fmt::format("+/- {:.{}f} {}", svA * du, dp, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f} {}", -svA * du, dp, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f} {}", svA * du, dp, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", svA * du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", svA * dd, dp);
        result.valUp = fmt::format("{:.{}f}", svA * (naturalBaseVal + du), dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", svA * (naturalBaseVal + du), dp);
        auto v2s = valueToString(naturalBaseVal, highPrecision);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";
        return result;
    }
    case A_TWO_TO_THE_B:
    {
        auto nvu = naturalBaseVal + modulationNatural;
        auto nvd = naturalBaseVal - modulationNatural;

        auto scv = svA * pow(2, svB * naturalBaseVal);
        auto svu = svA * pow(2, svB * nvu);
        auto svd = svA * pow(2, svB * nvd);
        auto du = svu - scv;
        auto dd = scv - svd;

        auto dp = (highPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f} {}", du, dp, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary = fmt::format("+/- {:.{}f} {}", du, dp, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f} {}", -du, dp, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f} {}", du, dp, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", dd, dp);
        result.valUp = fmt::format("{:.{}f}", nvu, dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", nvd, dp);
        auto v2s = valueToString(naturalBaseVal, highPrecision);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";
        return result;
    }
    default:
        break;
    }

    return {};
}

inline std::optional<float>
ParamMetaData::modulationNaturalFromString(const std::string_view &deltaNatural,
                                           float naturalBaseVal, std::string &errMsg) const
{
    switch (displayScale)
    {
    case LINEAR:
    {
        try
        {
            auto mv = std::stof(std::string(deltaNatural)) / svA;
            if (abs(mv) > (maxVal - minVal))
            {
                errMsg = fmt::format("Maximum depth: {} {}", (maxVal - minVal) * svA, unit);
                return {};
            }
            return mv;
        }
        catch (const std::exception &e)
        {
            return {};
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto xbv = svA * pow(2, svB * naturalBaseVal);
            auto mv = std::stof(std::string(deltaNatural));
            auto rv = xbv + mv;
            if (rv < 0)
            {
                return {};
            }

            auto r = log2(rv / svA) / svB;
            auto rg = maxVal - minVal;
            if (r < -rg || r > rg)
            {
                return {};
            }

            return r - naturalBaseVal;
        }
        catch (const std::exception &e)
        {
            return {};
        }
    }
    break;
    default:
        break;
    }
    return {};
}
} // namespace sst::basic_blocks::params

#endif