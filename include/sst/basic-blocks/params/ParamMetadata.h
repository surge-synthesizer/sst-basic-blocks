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

#ifndef INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H
#define INCLUDE_SST_BASIC_BLOCKS_PARAMS_PARAMMETADATA_H

/*
 * ParamMetaData is exactly that; a way to encode the metadata (range, scale, string
 * formtting, string parsing, etc...) for a parameter without specifying how to store
 * an actual runtime value. It is a configuration- and ui- time object mostly which
 * lets you advertise things like natural mins and maxes.
 *
 * Critically it does *not* store the data for a parameter. All the APIs assume the
 * actual value and configuration come from an external source, so multiple clients
 * can adapt to objects which advertise lists of these, like the sst- effects currently
 * do.
 *
 * The coding structure is basically a bunch of bool and value and enum members
 * and then a bunch of modifiers to set them (.withRange(min,max)) or to set clusters
 * of them (.asPercentBipolar()). We can add and expand these methods as we see fit.
 *
 * Please note this class is still a work in active development. There's an lot
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
 */

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cassert>
#include <cmath>
#include <cstdint>

#include <fmt/core.h>
#include <array>

namespace sst::basic_blocks::params
{
struct ParamMetaData
{
    ParamMetaData() = default;

    enum Type
    {
        FLOAT, // min/max/default value are in natural units
        INT,   // min/max/default value are in natural units, stored as a float. (int)round(val)
        BOOL,  // min/max 0/1. `val > 0.5` is true false test
        NONE   // special signifier that this param has no value. Used for structural things like
               // unused slots
    } type{FLOAT};

    std::string name;
    std::string groupName{}; // optional one level grouping, like VST3, AU and CLAP.

    uint32_t id{0};    // optional cache of an integer ID for plugin projection.
    uint32_t flags{0}; // optional flags to pass to a subsequent interpreter. Used by CLAP

    float minVal{0.f}, maxVal{1.f}, defaultVal{0.f};
    bool canExtend{false}, canDeform{false}, canAbsolute{false}, canTemposync{false},
        canDeactivate{false};
    float temposyncMultiplier{1.f};

    int deformationCount{0};

    bool supportsStringConversion{false};

    // Polarity can either be explicit set or inferred from the underling min max
    enum struct Polarity
    {
        INFERRED,          // figure out from min-max
        UNIPOLAR_POSITIVE, // 0 .. x
        UNIPOLAR_NEGATIVE, // -x .. 0
        BIPOLAR,           // -x .. x
        NO_POLARITY        // x .. y not meeting above conditions
    } polarity{Polarity::INFERRED};

    Polarity getPolarity() const
    {
        if (polarity != Polarity::INFERRED)
            return polarity;
        if (minVal == 0 && maxVal > 0)
            return Polarity::UNIPOLAR_POSITIVE;
        if (minVal < 0 && maxVal == 0)
            return Polarity::UNIPOLAR_NEGATIVE;
        if (minVal == -maxVal)
            return Polarity::BIPOLAR;
        return Polarity::NO_POLARITY;
    }

    bool isBipolar() const { return getPolarity() == Polarity::BIPOLAR; }
    bool isUnipolar() const
    {
        auto p = getPolarity();
        return p == Polarity::UNIPOLAR_NEGATIVE || p == Polarity::UNIPOLAR_POSITIVE;
    }

    /*
     * Parameters have an optional quantization which clients can use to
     * have jogs, quantized drags, and so forth. Right now we support three
     * baseic forms. None, a custom interval (so each drag is the interval in
     * natural units) or a custom step (so the space is divided into that many
     * steps). Standard with() functions to set it up are below.
     */
    enum struct Quantization
    {
        NO_QUANTIZATION,
        CUSTOM_INTERVAL,  // Quantize to interval
        CUSTOM_STEP_COUNT // this manyu steps. Basically interval = max-min/step
    } quantization{Quantization::NO_QUANTIZATION};
    float quantizationParam{0.f};

    bool supportsQuantization() const { return quantization != Quantization::NO_QUANTIZATION; }
    float quantize(float f) const
    {
        if (quantization == Quantization::CUSTOM_INTERVAL ||
            quantization == Quantization::CUSTOM_STEP_COUNT)
        {
            switch (displayScale)
            {
            case CUBED_AS_DECIBEL:
            {
                assert(quantization == Quantization::CUSTOM_INTERVAL);
                // so we have the db = 20 log10(val^3 * svA);
                auto v3 = f * f * f * svA;
                auto db = 20 * std::log10(v3);
                auto quantdb = quantizationParam * std::round(db / quantizationParam);
                auto quantv3 = pow(10.f, quantdb / 20);
                auto lv = std::cbrt(quantv3 / svA);
                return lv;
            }
            break;
            default:
            {
                auto dI = quantization == Quantization::CUSTOM_INTERVAL
                              ? quantizationParam
                              : ((maxVal - minVal) / quantizationParam);
                return dI * std::round(f / dI);
            }
            }
        }

        return f;
    }
    ParamMetaData withIntegerQuantization() const { return withQuantizedInterval(1.f); }
    ParamMetaData withQuantizedInterval(float interval) const
    {
        auto res = *this;
        res.quantization = Quantization::CUSTOM_INTERVAL;
        res.quantizationParam = interval;
        return res;
    }

    ParamMetaData withQuantizedStepCount(int steps) const
    {
        auto res = *this;
        res.quantization = Quantization::CUSTOM_STEP_COUNT;
        res.quantizationParam = steps;
        return res;
    }

    /*
     * Enabled indicates whether or not this parameter is used by the provider.
     * This is handy in dynamic situations where theres inter-param action, ike
     * the shrot circuit VA oscillator
     */
    bool enabled{true};
    bool isEnabled() const { return enabled; }
    ParamMetaData withEnabled(bool e)
    {
        auto res = *this;
        res.enabled = e;
        return res;
    }

    /*
     * Parameters have an extensible optional set of features stored in a single
     * uint64_t which you can flag on and off. This allows us to add things we want
     * as binaries on params without adding a squillion little bools for lesser importance
     * information only toggles. These features are not stream-at-rest stable just
     * stream-in-session stable by integer.
     */
    enum struct Features : uint64_t
    {
        SUPPORTS_MULTIPLICATIVE_MODULATION = 1ULL << 0,
        BELOW_ONE_IS_INVERSE_FRACTION = 1ULL << 1,
        ALLOW_FRACTIONAL_TYPEINS = 1ULL << 2,
        ALLOW_TUNING_FRACTION_TYPEINS = 1ULL << 3,

        USER_FEATURE_0 = 1ULL << 32
    };
    uint64_t features{0};
    ParamMetaData withFeature(Features f) const
    {
        auto res = *this;
        res.features |= (uint64_t)f;
        return res;
    }
    ParamMetaData withFeature(uint64_t f) const
    {
        auto res = *this;
        res.features |= f;
        return res;
    }
    bool hasFeature(Features f) const { return features & (uint64_t)f; }
    bool hasFeature(uint64_t f) const { return features & (uint64_t)f; }

    ParamMetaData withSupportsMultiplicativeModulation() const
    {
        return withFeature(Features::SUPPORTS_MULTIPLICATIVE_MODULATION);
    }
    bool hasSupportsMultiplicativeModulation() const
    {
        return hasFeature(Features::SUPPORTS_MULTIPLICATIVE_MODULATION);
    }

    /*
     * To String and From String conversion functions require information about the
     * parameter to execute. The primary driver is the value so the API takes the form
     * `valueToString(float)` but for optional features like extension, deform,
     * absolute and temposync we need to know that. Since this metadata does not
     * store any values but has the values handed externally, we could either have
     * a long-argument-list API or a little helper class for those values. We chose
     * the later in FeatureState
     */
    struct FeatureState
    {
        bool isHighPrecision{false}, isExtended{false}, isAbsolute{false}, isTemposynced{false},
            isNoUnits{false}, modulationClamped{true};

        FeatureState() {}

        FeatureState withHighPrecision(bool e)
        {
            auto res = *this;
            res.isHighPrecision = e;
            return res;
        }
        FeatureState withExtended(bool e)
        {
            auto res = *this;
            res.isExtended = e;
            return res;
        }
        FeatureState withAbsolute(bool e)
        {
            auto res = *this;
            isAbsolute = e;
            return res;
        }
        FeatureState withTemposync(bool e)
        {
            auto res = *this;
            res.isTemposynced = e;
            return res;
        }
        FeatureState withNoUnits(bool e)
        {
            auto res = *this;
            res.isNoUnits = e;
            return res;
        }
        FeatureState withModulationClamped(bool e)
        {
            auto res = *this;
            res.modulationClamped = e;
            return res;
        }
    };

    /*
     * What is the primary string representation of this value
     */
    std::optional<std::string> valueToString(float val, const FeatureState &fs = {}) const;

    /*
     * Some parameters have a secondary representation. For instance 441.2hz could also be ~A4.
     * If this parameter supports that it will return a string value from this API. Surge uses
     * this in the left side of the tooltip.
     */
    std::optional<std::string> valueToAlternateString(float val) const;

    /*
     * Convert a value to a string; if the optional is empty populate the error message.
     */
    std::optional<float> valueFromString(std::string_view, std::string &errMsg,
                                         const FeatureState &fs = {}) const;

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

        // baseValue, valUp/Dn and changeUp/Dn are no unit indications of change in each
        // direction.
        std::string baseValue, valUp, valDown, changeUp, changeDown;

        // modulationSummary is a longer-than-menu display suitable for single line infowindows
        std::string singleLineModulationSummary;
    };
    std::optional<ModulationDisplay> modulationNaturalToString(float naturalBaseVal,
                                                               float modulationNatural,
                                                               bool isBipolar,
                                                               const FeatureState &fs = {}) const;
    std::optional<float> modulationNaturalFromString(std::string_view deltaNatural,
                                                     float naturalBaseVal,
                                                     std::string &errMsg) const;

    enum DisplayScale
    {
        LINEAR,            // out = A * r + B
        A_TWO_TO_THE_B,    // out = A 2^(B r + C) + D
        CUBED_AS_DECIBEL,  // the underlier is an amplitude applied as v*v*v and displayed as db
        SCALED_OFFSET_EXP, // (exp(A + x ( B - A )) + C) / D
        DECIBEL,           // TODO - implement
        UNORDERED_MAP,     // out = discreteValues[(int)std::round(val)]
        MIDI_NOTE,    // uses C4 etc.. notation. The octaveOffset has 0 -> 69=A4, 1 = A5, -1 = A3
        LOGARITHMIC,  // A ln(v) / ln(B) + C
        USER_PROVIDED // TODO - implement
    } displayScale{LINEAR};

    std::string unit;
    std::string unitSeparator{" "};
    std::vector<std::tuple<std::string, float, float>> customValueLabelsWithAccuracy;

    std::unordered_map<int, std::string> discreteValues;
    int decimalPlaces{2};
    inline static int defaultMidiNoteOctaveOffset{0};
    int midiNoteOctaveOffset{defaultMidiNoteOctaveOffset};

    float svA{0.f}, svB{0.f}, svC{0.f}, svD{0.f}; // for various functional forms
    float exA{1.f}, exB{0.f};

    enum AlternateScaleWhen
    {
        NO_ALTERNATE,
        SCALE_BELOW,
        SCALE_ABOVE
    } alternateScaleWhen{NO_ALTERNATE};

    double alternateScaleCutoff{0.f}, alternateScaleRescaling{0.f};
    std::string alternateScaleUnits{};

    float naturalToNormalized01(float naturalValue, bool useSurgeIntConvention = false) const
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
            if (useSurgeIntConvention)
            {
                // This is the surge conversion. Do we want to keep it?
                v = 0.005 + 0.99 * (naturalValue - minVal) / (maxVal - minVal);
            }
            else
            {
                v = (naturalValue - minVal) / (maxVal - minVal);
            }
            break;
        case BOOL:
            assert(maxVal == 1 && minVal == 0);
            v = (naturalValue > 0.5 ? 1.f : 0.f);
            break;
        case NONE:
            assert(false);
            v = 0.f;
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
        case NONE:
            assert(false);
            return 0.f;
        }
        // quiet gcc
        return 0.f;
    }

    ParamMetaData withType(Type t)
    {
        auto res = *this;
        res.type = t;
        return res;
    }
    ParamMetaData asFloat()
    {
        auto res = *this;
        res.type = FLOAT;
        return res;
    }
    ParamMetaData asInt()
    {
        auto res = *this;
        res.type = INT;
        return res;
    }
    ParamMetaData asBool()
    {
        auto res = *this;
        res.type = BOOL;
        res.minVal = 0;
        res.maxVal = 1;
        return res;
    }
    ParamMetaData asOnOffBool() { return asBool().withOnOffFormatting(); }
    ParamMetaData asStereoSwitch() { return asOnOffBool().withName("Stereo"); }
    ParamMetaData withName(const std::string t)
    {
        auto res = *this;
        res.name = t;
        return res;
    }
    ParamMetaData withGroupName(const std::string t)
    {
        auto res = *this;
        res.groupName = t;
        return res;
    }
    ParamMetaData withID(const uint32_t id)
    {
        auto res = *this;
        res.id = id;
        return res;
    }
    ParamMetaData withFlags(const uint32_t f)
    {
        auto res = *this;
        res.flags = f;
        return res;
    }
    ParamMetaData withRange(float mn, float mx)
    {
        auto res = *this;
        res.minVal = mn;
        res.maxVal = mx;
        res.defaultVal = std::clamp(defaultVal, minVal, maxVal);
        return res;
    }
    ParamMetaData withDefault(float t)
    {
        auto res = *this;
        res.defaultVal = t;
        return res;
    }
    ParamMetaData withPolarity(Polarity p)
    {
        auto res = *this;
        res.polarity = p;
        return res;
    }

    ParamMetaData withTemposyncMultiplier(float f)
    {
        auto res = *this;
        res.temposyncMultiplier = f;
        return res;
    }
    ParamMetaData extendable(bool b = true)
    {
        auto res = *this;
        res.canExtend = b;
        return res;
    }
    // extend is val = (A * val) + B
    ParamMetaData withExtendFactors(float A, float B = 0.f)
    {
        auto res = *this;
        res.exA = A;
        res.exB = B;
        return res;
    }
    ParamMetaData deformable(bool b = true)
    {
        auto res = *this;
        res.canDeform = b;
        return res;
    }
    ParamMetaData withDeformationCount(int c)
    {
        auto res = *this;
        res.deformationCount = c;
        return res;
    }
    ParamMetaData absolutable(bool b = true)
    {
        auto res = *this;
        res.canAbsolute = b;
        return res;
    }
    ParamMetaData temposyncable(bool b = true)
    {
        auto res = *this;
        res.canTemposync = b;
        return res;
    }
    ParamMetaData deactivatable(bool b = true)
    {
        auto res = *this;
        res.canDeactivate = b;
        return res;
    }

    ParamMetaData withATwoToTheBFormatting(float A, float B, std::string_view units)
    {
        return withATwoToTheBPlusCFormatting(A, B, 0.f, units);
    }

    ParamMetaData withATwoToTheBPlusCFormatting(float A, float B, float C, std::string_view units)
    {
        return withATwoToTheBPlusCPlusDFormatting(A, B, C, 0.f, units);
    }

    ParamMetaData withATwoToTheBPlusCPlusDFormatting(float A, float B, float C, float D,
                                                     std::string_view units)
    {
        auto res = *this;
        res.svA = A;
        res.svB = B;
        res.svC = C;
        res.svD = D;
        res.unit = units;
        res.displayScale = A_TWO_TO_THE_B;
        res.supportsStringConversion = true;
        return res;
    }

    // A e^ (Bx + C) + d
    ParamMetaData withAExpBPlusCPlusDFormatting(float A, float B, float C, float D,
                                                std::string_view units)
    {
        // so e^x = 2^x/ln(2)
        // e^(BX + C) = 2^(BX + C)/ln(2)
        static constexpr float ln2{0.693147180559945};
        auto res = *this;
        res.svA = A;
        res.svB = B / ln2;
        res.svC = C / ln2;
        res.svD = D;
        res.unit = units;
        res.displayScale = A_TWO_TO_THE_B;
        res.supportsStringConversion = true;
        return res;
    }

    ParamMetaData withOBXFLogScale(float min, float max, float rolloff, std::string_view units)
    {
        // return ((expf(param * logf(rolloff + 1.f)) - 1.f) / (rolloff)) * (max - min) + min;
        // (e^(x log(ro+1)) - 1) * (max-min)/rolloff + min
        // A == (max-min)/rolloff
        // A e^(x log(ro+1)) - A + D
        auto At = (max - min) / rolloff;
        auto Bt = log(rolloff + 1);
        auto Dt = min - At;
        return withAExpBPlusCPlusDFormatting(At, Bt, 0, Dt, units);
    }

    ParamMetaData withScaledOffsetExpFormatting(float A, float B, float C, float D,
                                                const std::string &units)
    {
        auto res = *this;
        res.svA = A;
        res.svB = B;
        res.svC = C;
        res.svD = D;
        res.unit = units;
        res.displayScale = SCALED_OFFSET_EXP;
        res.supportsStringConversion = true;
        return res;
    }

    ParamMetaData withSemitoneZeroAt400Formatting()
    {
        return withATwoToTheBFormatting(440, 1.0 / 12.0, "Hz");
    }
    ParamMetaData withSemitoneZeroAtMIDIZeroFormatting()
    {
        return withATwoToTheBFormatting(440.f * pow(2.f, -69 / 12), 1.0 / 12.0, "Hz");
    }
    ParamMetaData withLog2SecondsFormatting() { return withATwoToTheBFormatting(1, 1, "s"); }

    ParamMetaData withLinearScaleFormatting(std::string units, float scale = 1.f,
                                            float offset = 0.f)
    {
        auto res = *this;
        res.svA = scale;
        res.svB = offset;
        res.unit = units;
        res.displayScale = LINEAR;
        res.supportsStringConversion = true;
        return res;
    }

    ParamMetaData withDimensionlessFormatting() { return withLinearScaleFormatting(""); }

    ParamMetaData withSemitoneFormatting()
    {
        return withLinearScaleFormatting("semitones")
            .withFeature(Features::ALLOW_TUNING_FRACTION_TYPEINS);
    }
    ParamMetaData withLogarithmicFormating(std::string units, float scale = 1,
                                           float basis = std::exp(0), float offset = 0)
    {
        auto res = *this;
        res.svA = scale;
        res.svB = basis;
        res.svC = offset;
        res.unit = units;
        res.displayScale = LOGARITHMIC;
        res.supportsStringConversion = true;
        return res;
    }

    ParamMetaData withMidiNoteFormatting(int octave)
    {
        auto res = *this;
        res.unit = "semitones";
        res.displayScale = MIDI_NOTE;
        res.supportsStringConversion = true;
        res.midiNoteOctaveOffset = octave;
        return res;
    }
    // Scan defaults to false because it needs to iterate through the map to find the value
    // range and client code could already know the appropriate min and max anyway
    ParamMetaData withUnorderedMapFormatting(const std::unordered_map<int, std::string> &map,
                                             bool scanAndInitParamRange = false)
    {
        auto res = *this;
        res.discreteValues = map;
        res.displayScale = UNORDERED_MAP;
        res.supportsStringConversion = true;
        if (scanAndInitParamRange)
        {
            auto valmax = std::numeric_limits<int>::min();
            auto valmin = std::numeric_limits<int>::max();
            for (const auto &e : map)
            {
                valmax = std::max(e.first, valmax);
                valmin = std::min(e.first, valmin);
            }
            res.minVal = valmin;
            res.maxVal = valmax;
        }
        res.type = INT;
        return res;
    }

    ParamMetaData withOnOffFormatting()
    {
        assert(type == BOOL || (type == INT && maxVal == 1 && minVal == 0));
        return withUnorderedMapFormatting({{false, "Off"}, {true, "On"}});
    }
    ParamMetaData withDecimalPlaces(int d)
    {
        auto res = *this;
        res.decimalPlaces = d;
        return res;
    }

    ParamMetaData withUnit(std::string_view s)
    {
        auto res = *this;
        res.unit = s;
        return res;
    }

    ParamMetaData withUnitSeparator(std::string_view s)
    {
        auto res = *this;
        res.unitSeparator = s;
        return res;
    }

    ParamMetaData withValueLabelRemoved(float v)
    {
        auto res = *this;

        auto r = res.customValueLabelsWithAccuracy.begin();
        while (r != res.customValueLabelsWithAccuracy.end())
        {
            if (std::get<1>(*r) == v)
            {
                r = res.customValueLabelsWithAccuracy.erase(r);
            }
            else
            {
                ++r;
            }
        }
        return res;
    }
    ParamMetaData withCustomMaxDisplay(const std::string &v)
    {
        return withCustomValueDisplay(v, maxVal, 1e-6);
    }

    ParamMetaData withCustomMinDisplay(const std::string &v)
    {
        return withCustomValueDisplay(v, minVal, 1e-6);
    }
    ParamMetaData withCustomDefaultDisplay(const std::string &v)
    {
        return withCustomValueDisplay(v, defaultVal);
    }

    ParamMetaData withCustomValueDisplay(const std::string &v, float val, float tol = 0.005)
    {
        auto res = withValueLabelRemoved(val);
        res.customValueLabelsWithAccuracy.emplace_back(v, val, tol);
        return res;
    }

    ParamMetaData withDisplayRescalingBelow(const float cutoff, const float rescale,
                                            const std::string &units)
    {
        auto res = *this;
        res.alternateScaleWhen = SCALE_BELOW;
        res.alternateScaleCutoff = cutoff;
        res.alternateScaleRescaling = rescale;
        res.alternateScaleUnits = units;
        return res;
    }

    ParamMetaData withoutDisplayRescaling()
    {
        auto res = *this;
        res.alternateScaleWhen = NO_ALTERNATE;
        return res;
    }

    ParamMetaData withMilisecondsBelowOneSecond()
    {
        auto res = withDisplayRescalingBelow(1.f, 1000.f, "ms");
        return res;
    }

    ParamMetaData withDisplayRescalingAbove(const float cutoff, const float rescale,
                                            const std::string &units)
    {
        auto res = *this;
        res.alternateScaleWhen = SCALE_ABOVE;
        res.alternateScaleCutoff = cutoff;
        res.alternateScaleRescaling = rescale;
        res.alternateScaleUnits = units;
        return res;
    }

    ParamMetaData asPercent()
    {
        return withRange(0.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withQuantizedInterval(0.1f)
            .withDecimalPlaces(2);
    }

    ParamMetaData asPercentExtendableToBipolar()
    {
        return asPercent().extendable().withExtendFactors(2.f, -1.f);
    }

    ParamMetaData asPercentBipolar()
    {
        return withRange(-1.f, 1.f)
            .withDefault(0.f)
            .withType(FLOAT)
            .withLinearScaleFormatting("%", 100.f)
            .withQuantizedInterval(0.1f)
            .withDecimalPlaces(2);
    }
    ParamMetaData asDecibelWithRange(float low, float high, float def = 0.f)
    {
        return withRange(low, high).withDefault(def).withType(FLOAT).withLinearScaleFormatting(
            "dB");
    }
    ParamMetaData asDecibelNarrow() { return asDecibelWithRange(-24, 24); }
    ParamMetaData asDecibel() { return asDecibelWithRange(-48, 48); }
    ParamMetaData asMIDIPitch()
    {
        return withType(FLOAT)
            .withRange(0.f, 127.f)
            .withDefault(60.f)
            .withLinearScaleFormatting("semitones")
            .withIntegerQuantization()
            .withDecimalPlaces(0);
    }
    ParamMetaData asMIDINote(int octave = 1000)
    {
        if (octave > 2 || octave < -2)
            octave = defaultMidiNoteOctaveOffset;

        return withType(INT)
            .withRange(0, 127)
            .withDefault(60)
            .withMidiNoteFormatting(octave)
            .withIntegerQuantization()
            .withDecimalPlaces(0);
    }
    ParamMetaData asLfoRate(float from = -7.f, float to = 9.f)
    {
        return withType(FLOAT)
            .withRange(from, to)
            .temposyncable()
            .withTemposyncMultiplier(-1)
            .withIntegerQuantization()
            .withATwoToTheBFormatting(1, 1, "Hz");
    }
    ParamMetaData asSemitoneRange(float lower = -96, float upper = 96)
    {
        return withType(FLOAT)
            .withRange(lower, upper)
            .withDefault(0)
            .withIntegerQuantization()
            .withLinearScaleFormatting("semitones");
    }
    ParamMetaData asLog2SecondsRange(float lower, float upper, float defVal = 0)
    {
        return withType(FLOAT)
            .withRange(lower, upper)
            .withDefault(std::clamp(defVal, lower, upper))
            .temposyncable()
            .withATwoToTheBFormatting(1, 1, "s")
            .withMilisecondsBelowOneSecond();
    }
    ParamMetaData asEnvelopeTime() { return asLog2SecondsRange(-8.f, 5.f, -1.f); }

    // (exp(lerp(norm_val, 0.6931471824646, 10.1267113685608)) - 2.0)/1000.0
    ParamMetaData as25SecondExpTime()
    {
        return withType(FLOAT)
            .withRange(0, 1)
            .withDefault(0.1)
            .withScaledOffsetExpFormatting(0.6931471824646, 10.1267113685608, -2.0, 1000.0, "s")
            .withMilisecondsBelowOneSecond();
    }

    ParamMetaData asAudibleFrequency()
    {
        return withType(FLOAT).withRange(-60, 70).withDefault(0).withSemitoneZeroAt400Formatting();
    }

    /**
     * A 0...1 value where the amplitude is the value cubed and the display is in
     * decibels
     */
    ParamMetaData asCubicDecibelAttenuation() { return asCubicDecibelUpTo(0.f); }

    /**
     * This creates a param with a max val different from 1, such that the value of
     * 1 is still 0db and the max val is the dbs provided
     * @param maxDB Decibels for the top of range
     */
    ParamMetaData asCubicDecibelAttenuationWithUpperDBBound(float maxDB)
    {
        auto res = asCubicDecibelAttenuation();
        auto ampmax = pow(10, maxDB / 20);
        auto mx = std::cbrt(ampmax);
        res.maxVal = mx;
        return res.withDefault(1.0);
    }

    /**
     * This assumes you have a 0...1 underlyer and want it to represent a
     * scaled range up to some decibels.
     *
     * @param maxDb the decibels represented by the '1' extram
     */
    ParamMetaData asCubicDecibelUpTo(float maxDb)
    {
        auto res = withType(FLOAT).withRange(0.f, 1.f).withDefault(1.f);
        res.displayScale = CUBED_AS_DECIBEL;
        res.supportsStringConversion = true;
        res.svA = pow(10.f, maxDb / 20.0);

        // find the 0db point
        // v * v * v * svA is the amp
        // db = 20 log10(amp)
        // 0 = 20 log10(v*v*v*svA)
        // v * v * v * svA = 1
        // v = cbrt(1/svA)
        res = res.withDefault(std::cbrt(1.f / res.svA)).withQuantizedInterval(3.f);

        res = res.withSupportsMultiplicativeModulation();
        return res;
    }
    ParamMetaData asLinearDecibel(float lower = -96, float upper = 12)
    {
        return withType(FLOAT)
            .withRange(lower, upper)
            .withDefault(0)
            .withIntegerQuantization()
            .withSupportsMultiplicativeModulation()
            .withLinearScaleFormatting("dB");
    }

    ParamMetaData asPan()
    {
        return asPercentBipolar()
            .withDefault(0)
            .withDecimalPlaces(0)
            .withCustomDefaultDisplay("C")
            .withCustomMaxDisplay("R")
            .withCustomMinDisplay("L");
    }

    // For now, his is the temposync notation assuming a 2^x and temposync ratio based on 120bpm
    std::string temposyncNotation(float f) const;
    std::optional<float> valueFromTemposyncNotation(const std::string &s) const;
    float snapToTemposync(float f) const;

    /*
     * OK so I'm doing something a bit tricky here. I want to be able to project
     * a PMD onto a clap_param_info * but I don't want to couple this low level library
     * to clap/ext/param.h. So I will duck type it with a template
     */
    template <int stringSize, typename IsAClapParamInfo>
    void toClapParamInfo(IsAClapParamInfo *info) const
    {
        info->id = id;
        strncpy(info->name, name.c_str(), stringSize - 2);
        info->name[stringSize - 1] = 0;
        strncpy(info->module, groupName.c_str(), stringSize - 2);
        info->module[stringSize - 1] = 0;
        info->min_value = minVal;
        info->max_value = maxVal;
        info->default_value = defaultVal;
        info->flags = flags;
    }
};

/*
 * Implementation below here
 */
inline std::optional<std::string> ParamMetaData::valueToString(float val,
                                                               const FeatureState &fs) const
{
    if (type == BOOL)
    {
        for (auto &v : customValueLabelsWithAccuracy)
        {
            if (std::get<1>(v) < 0.1 && val < 0.5)
                return std::get<0>(v);

            if (std::get<1>(v) > 0.9 && val > 0.5)
                return std::get<0>(v);
        }
    }

    if (type == INT)
    {
        for (auto &det : customValueLabelsWithAccuracy)
        {
            auto dv = std::get<1>(det);
            auto da = std::get<2>(det);

            if (fabs(val - dv) < da * (maxVal - minVal))
                return std::get<0>(det);
        }

        auto iv = (int)std::round(val);
        if (displayScale == UNORDERED_MAP)
        {
            if (discreteValues.find(iv) != discreteValues.end())
                return discreteValues.at(iv);
            return std::nullopt;
        }
        if (displayScale == MIDI_NOTE)
        {
            if (iv < 0)
                return "";
            auto n = iv;
            auto o = n / 12 - 1 + midiNoteOctaveOffset;
            auto ni = n % 12;
            static std::array<std::string, 12> nn{"C",  "C#", "D",  "D#", "E",  "F",
                                                  "F#", "G",  "G#", "A",  "A#", "B"};

            auto res = nn[ni] + std::to_string(o);

            return res;
        }
        if (displayScale == LINEAR)
        {
            if (fs.isNoUnits)
                return std::to_string(iv);

            return std::to_string(iv) + (unit.empty() ? "" : unitSeparator) + unit;
        }

        return std::nullopt;
    }

    for (auto &det : customValueLabelsWithAccuracy)
    {
        auto dv = std::get<1>(det);
        auto da = std::get<2>(det);

        if (fabs(val - dv) < da * (maxVal - minVal))
            return std::get<0>(det);
    }

    if (fs.isExtended)
        val = exA * val + exB;

    if (fs.isTemposynced)
    {
        return temposyncNotation(snapToTemposync(temposyncMultiplier * val));
    }

    // float cases
    switch (displayScale)
    {
    case LINEAR:
        if (alternateScaleWhen == NO_ALTERNATE)
        {
            if (fs.isNoUnits)
            {
                auto res = fmt::format("{:.{}f}", svA * val + svB,
                                       (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces));
                return res;
            }
            else
            {
                return fmt::format("{:.{}f}{}{:s}", svA * val + svB,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, unit);
            }
        }
        else
        {
            // Conscious choice - don't supress units if alternate units are in effect
            assert(!fs.isNoUnits);
            auto rsv = svA * val;
            if ((alternateScaleWhen == SCALE_BELOW && rsv < alternateScaleCutoff) ||
                (alternateScaleWhen == SCALE_ABOVE && rsv > alternateScaleCutoff))
            {
                rsv = rsv * alternateScaleRescaling;
                return fmt::format("{:.{}f}{}{:s}", rsv,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, alternateScaleUnits);
            }
            else
            {
                return fmt::format("{:.{}f}{}{:s}", svA * val,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, unit);
            }
        }
        break;
    case A_TWO_TO_THE_B:
        if (alternateScaleWhen == NO_ALTERNATE)
        {
            auto dval = svA * pow(2.0, svB * val + svC) + svD;
            std::string prefix{};
            if ((features & (uint64_t)Features::BELOW_ONE_IS_INVERSE_FRACTION) && dval < 1 &&
                dval > 0)
            {
                dval = 1.0 / dval;
                prefix = "1/";
            }
            if (fs.isNoUnits)
            {
                return fmt::format("{}{:.{}f}", prefix, dval,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces));
            }
            else
            {
                return fmt::format("{}{:.{}f}{}{:s}", prefix, dval,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, unit);
            }
        }
        else
        {
            // Conscious choice - don't supress units if alternate units are in effect
            assert(!fs.isNoUnits);

            auto rsv = svA * pow(2.0, svB * val + svC) + svD;
            if ((alternateScaleWhen == SCALE_BELOW && rsv < alternateScaleCutoff) ||
                (alternateScaleWhen == SCALE_ABOVE && rsv > alternateScaleCutoff))
            {
                rsv = rsv * alternateScaleRescaling;
                return fmt::format("{:.{}f}{}{:s}", rsv,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, alternateScaleUnits);
            }
            else
            {
                return fmt::format("{:.{}f}{}{:s}", rsv,
                                   (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                                   unitSeparator, unit);
            }
        }
        break;
    case LOGARITHMIC:
    {
        if (val <= 0)
            return "-inf";
        auto dval = svA * std::log(val) / std::log(svB) + svC;

        if (fs.isNoUnits)
        {
            return fmt::format("{:.{}f}", dval,
                               (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces));
        }
        else
        {
            return fmt::format("{:.{}f}{}{:s}", dval,
                               (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                               unitSeparator, unit);
        }
    }
    break;
    case SCALED_OFFSET_EXP:
    {
        auto dval = (std::exp(svA + val * (svB - svA)) + svC) / svD;
        if (alternateScaleWhen == NO_ALTERNATE ||
            (alternateScaleWhen == SCALE_BELOW && dval > alternateScaleCutoff) ||
            (alternateScaleWhen == SCALE_ABOVE && dval < alternateScaleCutoff))
        {
            return fmt::format("{:.{}f}{}{:s}", dval,
                               (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                               unitSeparator, unit);
        }
        // We must be in an alternate case
        return fmt::format("{:.{}f}{}{:s}", dval * alternateScaleRescaling,
                           (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                           unitSeparator, alternateScaleUnits);
    }
    break;
    case CUBED_AS_DECIBEL:
    {
        if (val <= 0)
        {
            return fmt::format("-inf{}dB", unitSeparator);
        }

        auto v3 = val * val * val * svA;
        auto db = 20 * std::log10(v3);
        return fmt::format("{:.{}f}{}", db,
                           (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces),
                           fs.isNoUnits ? "" : unitSeparator + "dB");
    }
    break;
    default:
        break;
    }
    return std::nullopt;
}

inline std::optional<float> ParamMetaData::valueFromString(std::string_view v, std::string &errMsg,
                                                           const FeatureState &fs) const
{
    if (type == BOOL)
    {
        if (v == "On" || v == "on" || v == "1" || v == "true" || v == "True")
            return 1.f;
        if (v == "Off" || v == "off" || v == "0" || v == "false" || v == "False")
            return 0.f;
    }
    if (type == INT)
    {
        if (displayScale == MIDI_NOTE)
        {
            auto s = std::string(v);
            auto c0 = std::toupper(s[0]);
            if (c0 >= 'A' && c0 <= 'G')
            {
                auto n0 = c0 - 'A';
                auto sharp = s[1] == '#';
                auto flat = s[1] == 'b';
                auto oct = std::atoi(s.c_str() + 1 + (sharp ? 1 : 0) + (flat ? 1 : 0));

                std::array<int, 7> noteToPosition{9, 11, 0, 2, 4, 5, 7};
                auto res =
                    noteToPosition[n0] + sharp - flat + (oct + 1 + midiNoteOctaveOffset) * 12;
                if (res >= minVal && res <= maxVal)
                    return (float)res;
            }
            else
            {
                auto res = (float)std::atoi(s.c_str());
                if (res >= minVal && res <= maxVal)
                    return res;
            }
        }
        if (displayScale == LINEAR)
        {
            auto res = std::atoi(std::string(v).c_str());
            if (res >= minVal && res <= maxVal)
                return res;
        }
        return std::nullopt;
    }

    for (const auto &det : customValueLabelsWithAccuracy)
    {
        if (v == std::get<0>(det))
            return std::get<1>(det);
    };

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
            auto r = 1.0;
            auto vs = std::string(v);

            auto isFrac = (features & (uint64_t)Features::ALLOW_FRACTIONAL_TYPEINS);
            auto isTunFrac = (features & (uint64_t)Features::ALLOW_TUNING_FRACTION_TYPEINS);
            if ((isFrac || isTunFrac) && vs.find("/") != std::string::npos)
            {
                auto ps = vs.find("/");
                auto num = vs.substr(0, ps);
                auto den = vs.substr(ps + 1);
                auto uv = std::stof(num);
                auto dv = std::stof(den);
                r = std::stof(std::string(v));
                if (isFrac && uv != 0 && dv != 0)
                {
                    r = uv / dv;
                }
                if (isTunFrac && dv != 0 && uv / dv > 0)
                {
                    r = 12 * log2(uv / dv);
                }
            }
            else
            {
                r = std::stof(std::string(v));
            }

            assert(svA != 0);
            r = (r - svB) / svA;

            if (alternateScaleWhen != NO_ALTERNATE)
            {
                auto unitSubstr = (unit.find(alternateScaleUnits) != std::string::npos);
                auto us = (v.find(unit) != std::string::npos);
                auto ps = (v.find(alternateScaleUnits) != std::string::npos);
                if ((!unitSubstr && ps) || (unitSubstr && ps && !us))
                {
                    // We have a string containing the alternAte units
                    r = r / alternateScaleRescaling;
                }
            }

            if (fs.isExtended)
            {
                r = (r - exB) / exA;
            }

            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto r = 1.0;
            auto vs = std::string(v);
            if ((features & (uint64_t)Features::ALLOW_FRACTIONAL_TYPEINS) &&
                vs.find("/") != std::string::npos)
            {
                auto ps = vs.find("/");
                auto num = vs.substr(0, ps);
                auto den = vs.substr(ps + 1);
                auto uv = std::stof(num);
                auto dv = std::stof(den);
                if (uv == 0 || dv == 0)
                    r = std::stof(std::string(v));
                else
                    r = uv / dv;
            }
            else if ((features & (uint64_t)Features::BELOW_ONE_IS_INVERSE_FRACTION) &&
                     vs.find("1/") != std::string::npos)
            {
                auto ps = vs.find("1/");
                auto ss = vs.substr(ps + 2);
                auto uv = std::stof(ss);
                if (uv == 0)
                    r = 1.;
                else
                    r = 1.0 / uv;
            }
            else
            {
                r = std::stof(std::string(v));
            }
            assert(svA != 0);
            assert(svB != 0);

            if (alternateScaleWhen != NO_ALTERNATE)
            {
                auto unitSubstr = (unit.find(alternateScaleUnits) != std::string::npos);
                auto us = (v.find(unit) != std::string::npos);
                auto ps = (v.find(alternateScaleUnits) != std::string::npos);
                if ((!unitSubstr && ps) || (unitSubstr && ps && !us))
                {
                    // We have a string containing the alternate units
                    r = r / alternateScaleRescaling;
                }
            }

            if (r < 0)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }
            /* v = svA 2^(svB r + svC) + svD
             * log2((v-svD) / svA) = svB r + svC
             * (log2((v-svD)/svA) - svC)/svB = r
             */
            r = (log2((r - svD) / svA) - svC) / svB;
            if (r < minVal || r > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return (float)r;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
    }
    break;
    case LOGARITHMIC:
    {
        if (v == "-inf")
            return minVal;

        auto r = std::stof(std::string(v));
        // A ln(r) / ln(B) + C = v
        // (r - c) * lnB / A = lnv
        auto lnv = (r - svC) * std::log(svB) / svA;
        auto res = std::exp(lnv);
        if (res < minVal || res > maxVal)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
        return res;
    }
    break;

    case SCALED_OFFSET_EXP:
    {
        try
        {
            auto r = std::stof(std::string(v));

            if (alternateScaleWhen != NO_ALTERNATE)
            {
                auto ps = v.find(alternateScaleUnits);
                if (ps != std::string::npos && alternateScaleRescaling != 0.f)
                {
                    // We have a string containing the alterante units
                    r = r / alternateScaleRescaling;
                }
            }

            // OK so its R = exp(A + X (B-A)) + C)/D
            // D R - C = exp(A + X (B-a))
            // log(DR - C) = A + X (B-A)
            // (log (DR - C) - A) / (B - A) = X
            auto drc = std::max(svD * r - svC, 0.00000001f);
            auto xv = (std::log(drc) - svA) / (svB - svA);

            if (xv < minVal || xv > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return xv;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
        return 0.f;
    }
    break;
    case CUBED_AS_DECIBEL:
    {
        try
        {
            if (v == "-inf")
                return 0.f;

            auto r = std::stof(std::string(v));
            auto db = pow(10.f, r / 20);
            auto lv = std::cbrt(db / svA);
            if (lv < minVal || lv > maxVal)
            {
                errMsg = rangeMsg();
                return std::nullopt;
            }

            return (float)lv;
        }
        catch (const std::exception &)
        {
            errMsg = rangeMsg();
            return std::nullopt;
        }
    }
    break;
    default:
        break;
    }
    return std::nullopt;
}

inline std::optional<std::string> ParamMetaData::valueToAlternateString(float) const
{
    return std::nullopt;
}

inline std::optional<ParamMetaData::ModulationDisplay>
ParamMetaData::modulationNaturalToString(float naturalBaseVal, float modulationNatural,
                                         bool isBipolar, const FeatureState &fs) const
{
    if (type != FLOAT)
        return std::nullopt;
    ModulationDisplay result;

    switch (displayScale)
    {
    case LINEAR:
    {
        assert(std::fabs(modulationNatural) <= maxVal - minVal);
        // OK this is super easy. It's just linear!
        auto du = modulationNatural;
        auto dd = -modulationNatural;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f}{}{}", svA * du + svB, dp, unitSeparator, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary =
                    fmt::format("+/- {:.{}f}{}{}", svA * du + svB, dp, unitSeparator, unit);
            }
            else
            {
                result.summary =
                    fmt::format("-/+ {:.{}f}{}{}", -svA * du + svB, dp, unitSeparator, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f}{}{}", svA * du + svB, dp, unitSeparator, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", svA * du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", svA * dd, dp);
        result.valUp = fmt::format("{:.{}f}", svA * (naturalBaseVal + du), dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", svA * (naturalBaseVal - du), dp);
        // TODO pass this on not create
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";

        if (isBipolar)
            result.singleLineModulationSummary =
                fmt::format("{}{}{} < {} > {}{}{}", result.valDown, unitSeparator, unit,
                            result.baseValue, result.valUp, unitSeparator, unit);
        else
            result.singleLineModulationSummary =
                fmt::format("{} > {}{}{}", result.baseValue, result.valUp, unitSeparator, unit);
        return result;
    }
    case A_TWO_TO_THE_B:
    {
        auto nvu = naturalBaseVal + modulationNatural;
        auto nvd = naturalBaseVal - modulationNatural;

        std::string upPfx{}, downPfx{};
        if (fs.modulationClamped)
        {
            if (nvu > maxVal)
            {
                nvu = maxVal;
                upPfx = ">";
            }
            if (nvu < minVal)
            {
                nvu = minVal;
                upPfx = "<";
            }
            if (nvd > maxVal)
            {
                nvd = maxVal;
                downPfx = ">";
            }
            if (nvd < minVal)
            {
                nvd = minVal;
                downPfx = "<";
            }
        }

        auto scv = svA * pow(2, svB * naturalBaseVal + svC) + svD;
        auto svu = svA * pow(2, svB * nvu + svC) + svD;
        auto svd = svA * pow(2, svB * nvd + svC) + svD;
        auto du = svu - scv;
        auto dd = scv - svd;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{}{:.{}f}{}{}", upPfx, du, dp, unitSeparator, unit);
        if (isBipolar)
        {
            if (du > 0)
            {
                result.summary = fmt::format("+/- {:.{}f}{}{}", du, dp, unitSeparator, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f}{}{}", -du, dp, unitSeparator, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f}{}{}", du, dp, unitSeparator, unit);
        }
        result.changeUp = fmt::format("{}{:.{}f}", upPfx, du, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{}{:.{}f}", downPfx, dd, dp);
        result.valUp = fmt::format("{}{:.{}f}", upPfx, svu, dp);

        if (isBipolar)
            result.valDown = fmt::format("{}{:.{}f}", downPfx, svd, dp);
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";

        if (isBipolar)
            result.singleLineModulationSummary =
                fmt::format("{}{}{}{} < {} > {}{}{}{}", downPfx, result.valDown, unitSeparator,
                            unit, result.baseValue, upPfx, result.valUp, unitSeparator, unit);
        else
            result.singleLineModulationSummary = fmt::format(
                "{} > {}{}{}{}", result.baseValue, upPfx, result.valUp, unitSeparator, unit);

        return result;
    }
    case SCALED_OFFSET_EXP:
    {
        auto nvu = std::clamp(naturalBaseVal + modulationNatural, 0.f, 1.f);
        auto nvd = std::clamp(naturalBaseVal - modulationNatural, 0.f, 1.f);
        auto nv = std::clamp(naturalBaseVal, 0.f, 1.f);

        auto v = (exp(svA + nv * (svB - svA)) - svC) / svD;
        auto vu = (exp(svA + nvu * (svB - svA)) - svC) / svD;
        auto vd = (exp(svA + nvd * (svB - svA)) - svC) / svD;

        auto deltUp = vu - v;
        auto deltDn = vd - v;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
        if (isBipolar)
        {
            if (deltDn > 0)
            {
                result.summary = fmt::format("+/- {:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f}{}{}", -deltUp, dp, unitSeparator, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", deltUp, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", deltDn, dp);
        result.valUp = fmt::format("{:.{}f}", vu, dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", vd, dp);
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";

        if (isBipolar)
            result.singleLineModulationSummary =
                fmt::format("{}{}{} < {} > {}{}{}", result.valDown, unitSeparator, unit,
                            result.baseValue, result.valUp, unitSeparator, unit);
        else
            result.singleLineModulationSummary =
                fmt::format("{} > {}{}{}", result.baseValue, result.valUp, unitSeparator, unit);

        return result;
    }
    break;
    case CUBED_AS_DECIBEL:
    {
        auto nvu = std::max(naturalBaseVal + modulationNatural, 0.f);
        auto nvd = std::max(naturalBaseVal - modulationNatural, 0.f);
        auto v = std::max(naturalBaseVal, 0.f);

        nvu = nvu * nvu * nvu * svA;
        nvd = nvd * nvd * nvd * svA;
        v = v * v * v * svA;

        auto db = 20 * std::log10(v);
        auto dbu = 20 * std::log10(nvu);
        auto dbd = 20 * std::log10(nvd);

        auto deltUp = dbu - db;
        auto deltDn = dbd - db;

        auto dp = (fs.isHighPrecision ? (decimalPlaces + 4) : decimalPlaces);
        result.value = fmt::format("{:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
        if (isBipolar)
        {
            if (deltDn > 0)
            {
                result.summary = fmt::format("+/- {:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
            }
            else
            {
                result.summary = fmt::format("-/+ {:.{}f}{}{}", -deltUp, dp, unitSeparator, unit);
            }
        }
        else
        {
            result.summary = fmt::format("{:.{}f}{}{}", deltUp, dp, unitSeparator, unit);
        }
        result.changeUp = fmt::format("{:.{}f}", deltUp, dp);
        if (isBipolar)
            result.changeDown = fmt::format("{:.{}f}", deltDn, dp);
        result.valUp = fmt::format("{:.{}f}", dbu, dp);

        if (isBipolar)
            result.valDown = fmt::format("{:.{}f}", dbd, dp);
        auto v2s = valueToString(naturalBaseVal, fs);
        if (v2s.has_value())
            result.baseValue = *v2s;
        else
            result.baseValue = "-ERROR-";

        if (isBipolar)
            result.singleLineModulationSummary =
                fmt::format("{}{}{} < {} > {}{}{}", result.valDown, unitSeparator, unit,
                            result.baseValue, result.valUp, unitSeparator, unit);
        else
            result.singleLineModulationSummary =
                fmt::format("{} > {}{}{}", result.baseValue, result.valUp, unitSeparator, unit);

        return result;
    }
    break;
    default:
        break;
    }

    return std::nullopt;
}

inline std::optional<float>
ParamMetaData::modulationNaturalFromString(std::string_view deltaNatural, float naturalBaseVal,
                                           std::string &errMsg) const
{
    switch (displayScale)
    {
    case LINEAR:
    {
        try
        {
            auto mv = std::stof(std::string(deltaNatural)) / svA;
            if (std::fabs(mv) > (maxVal - minVal))
            {
                errMsg = fmt::format("Maximum depth: {}{}{}", (maxVal - minVal) * svA,
                                     unitSeparator, unit);
                return std::nullopt;
            }
            return mv;
        }
        catch (const std::exception &e)
        {
            return std::nullopt;
        }
    }
    break;
    case A_TWO_TO_THE_B:
    {
        try
        {
            auto xbv = svA * pow(2, svB * naturalBaseVal) + svD;
            auto mv = std::stof(std::string(deltaNatural));
            auto rv = xbv + mv;
            if (rv < 0)
            {
                return std::nullopt;
            }

            auto r = log2(rv / svA) / svB;
            auto rg = maxVal - minVal;
            if (r < -rg || r > rg)
            {
                return std::nullopt;
            }

            return (float)(r - naturalBaseVal);
        }
        catch (const std::exception &e)
        {
            errMsg = "Unable to convert " + std::string(deltaNatural) + " to a float";
            return std::nullopt;
        }
    }
    break;
    case CUBED_AS_DECIBEL:
    {
        try
        {
            auto bv = naturalBaseVal * naturalBaseVal * naturalBaseVal * svA;
            auto db = 20 * std::log10(bv);
            auto mv = std::stof(std::string(deltaNatural));
            auto rv = db + mv;
            auto av = std::cbrt(pow(10.f, rv / 20) / svA);
            return (av - naturalBaseVal);
        }
        catch (const std::exception &e)
        {
            errMsg = "Unable to convert " + std::string(deltaNatural) + " to a float";
            return std::nullopt;
        }
    }
    break;
    case SCALED_OFFSET_EXP:
    {
        try
        {
            auto nv = std::clamp(naturalBaseVal, 0.f, 1.f);
            auto v = (exp(svA + nv * (svB - svA)) - svC) / svD;
            auto mv = std::stof(std::string(deltaNatural));
            auto rv = v + mv;
            // See comment in valueFromString for the algebra here
            auto drc = std::max((float)(svD * rv - svC), 0.00000001f);
            auto xv = (std::log(drc) - svA) / (svB - svA);
            auto dist = xv - naturalBaseVal;
            return dist;
        }
        catch (const std::exception &e)
        {
            errMsg = "Unable to convert " + std::string(deltaNatural) + " to a float";
            return std::nullopt;
        }
    }
    break;
    default:
        errMsg = "Display scale " + std::to_string(displayScale) +
                 " not supported in modulation from string";
        break;
    }
    return std::nullopt;
}

inline std::string ParamMetaData::temposyncNotation(float f) const
{
    assert(type == FLOAT);
    assert(displayScale == A_TWO_TO_THE_B);
    assert(svD == 0.f);
    float a, b = modff(f, &a);

    if (b >= 0)
    {
        b -= 1.0;
        a += 1.0;
    }

    float d, q;
    std::string nn, t;
    char tmp[1024];

    if (f >= 1)
    {
        q = std::pow(2.0f, f - 1);
        nn = "whole";
        if (q >= 3)
        {
            if (std::fabs(q - floor(q + 0.01)) < 0.01)
            {
                snprintf(tmp, 1024, "%d whole notes", (int)floor(q + 0.01));
            }
            else
            {
                // this is the triplet case
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
            }
            std::string res = tmp;
            return res;
        }
        else if (q >= 2)
        {
            nn = "double whole";
            q /= 2;
        }

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            if (nn == "whole")
            {
                nn = "double whole";
            }
            else
            {
                q = pow(2.0, f - 1);
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
                std::string res = tmp;
                return res;
            }
        }
        else
        {
            t = "dotted";
        }
    }
    else
    {
        d = pow(2.0, -(a - 2));
        q = pow(2.0, (b + 1));

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            d = d / 2;
        }
        else
        {
            t = "dotted";
        }
        if (d == 1)
        {
            nn = "whole";
        }
        else
        {
            char tmp[1024];
            snprintf(tmp, 1024, "1/%d", (int)d);
            nn = tmp;
        }
    }
    std::string res = nn + " " + t;

    return res;
}

inline std::optional<float> ParamMetaData::valueFromTemposyncNotation(const std::string &s) const
{
    // OK so the basic idea is marching down the string we have numbers and slashes
    // then characters from a constrained set
    std::string numPart;
    std::string restPart;
    bool inNum{true};
    for (const auto &c : s)
    {
        if (inNum && ((c >= '0' && c <= '9') || c == '/' || c == ' '))
        {
            numPart += c;
        }
        else
        {
            inNum = false;
            if (c != ' ')
                restPart += std::toupper(c);
        }
    }
    if (numPart.empty())
        return std::nullopt;

    auto slpos = numPart.find('/');
    auto num = 0, den = 1;
    if (slpos == std::string::npos)
    {
        num = std::stoi(numPart);
    }
    else
    {
        num = std::stoi(numPart.substr(0, slpos));
        den = std::stoi(numPart.substr(slpos + 1));
    }
    if (den == 0)
        return std::nullopt;

    // 1/2 should be 0, 1/4 should be 1
    auto frac = 2.0 * num / den;
    auto ufrac = 1.0 / frac;
    auto pfrac = std::floor(std::log2(ufrac));

    if (restPart == "T")
        pfrac += 0.51;
    if (restPart == "D" || restPart == ".")
    {
        pfrac -= 0.6;
    }

    return snapToTemposync(pfrac);
}

inline float ParamMetaData::snapToTemposync(float f) const
{
    assert(canTemposync);
    assert(type == FLOAT);
    assert(displayScale == A_TWO_TO_THE_B);
    float a, b = modff(f, &a);
    if (b < 0)
    {
        b += 1.f;
        a -= 1.f;
    }
    b = powf(2.0f, b);

    if (b > 1.41f)
    {
        b = log2(1.5f);
    }
    else if (b > 1.167f)
    {
        b = log2(1.3333333333f);
    }
    else
    {
        b = 0.f;
    }

    return a + b;
}

} // namespace sst::basic_blocks::params

#endif
