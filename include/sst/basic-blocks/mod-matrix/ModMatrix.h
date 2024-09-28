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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MOD_MATRIX_MODMATRIX_H
#define INCLUDE_SST_BASIC_BLOCKS_MOD_MATRIX_MODMATRIX_H

#include <optional>
#include <array>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdlib>
#include <cmath>

#include <iostream>
#include "ModMatrixDetails.h"
#include "../dsp/Lag.h"

/*
 * This is an implementation of a relatively genericised mod matrix which
 * has the form of 'source1 * source2 * depth' if active. The values for the
 * target and source ar a template trait which have a value semantic and can
 * hash and operator==.
 *
 * The matrix provides pointers to values at runtime and binds to input target
 * base value and source pointers. It's assumed these pointers live longer
 * than the matrix.
 */
namespace sst::basic_blocks::mod_matrix
{

enum ApplicationMode
{
    ADDITIVE = 0,
    MULTIPLICATIVE = 1
};

template <typename ModMatrixTraits>
struct RoutingTable : details::CheckModMatrixConstraints<ModMatrixTraits>
{
    using TR = ModMatrixTraits;

    struct Routing
    {
        bool active{true};
        std::optional<typename TR::SourceIdentifier> source{std::nullopt};
        std::optional<typename TR::SourceIdentifier> sourceVia{std::nullopt};
        std::optional<typename TR::TargetIdentifier> target{std::nullopt};
        std::optional<typename TR::CurveIdentifier> curve{std::nullopt};

        // We use integers as milisecond lag values so we can compare with 0
        int16_t sourceLagMS{0}, sourceViaLagMS{0};
        bool sourceLagExp{true}, sourceViaLagExp{true};

        int16_t applicationMode{ADDITIVE};

        float depth{0.f};

        std::optional<typename TR::RoutingExtraPayload> extraPayload{std::nullopt};

        bool hasDefaultValues() const
        {
            auto res = active;
            res = res && (depth == 0.f);
            res = res && (!source.has_value());
            res = res && (!sourceVia.has_value());
            res = res && (!target.has_value());
            res = res && (!curve.has_value());
            res = res && (!extraPayload.has_value());
            return res;
        }
    };
};

template <typename ModMatrixTraits>
struct ModMatrix : details::CheckModMatrixConstraints<ModMatrixTraits>
{
    using TR = ModMatrixTraits;

    std::unordered_map<typename TR::TargetIdentifier, float &> baseValues;
    void bindTargetBaseValue(const typename TR::TargetIdentifier &t, float &f)
    {
        baseValues.erase(t);
        baseValues.insert_or_assign(t, f);
    }

    std::unordered_map<typename TR::SourceIdentifier, float &> sourceValues;
    std::unordered_map<typename TR::SourceIdentifier, float> constantPlaceholders;
    void bindSourceValue(const typename TR::SourceIdentifier &s, float &f)
    {
        sourceValues.erase(s);
        sourceValues.insert_or_assign(s, f);
    }

    void bindSourceConstantValue(const typename TR::SourceIdentifier &c, float value)
    {
        constantPlaceholders[c] = value;
        bindSourceValue(c, constantPlaceholders[c]);
    }

    static constexpr bool canSelfModulate{details::has_isTargetModMatrixDepth<TR>::value};
    bool isTargetModMatrixDepth(typename TR::TargetIdentifier &t)
    {
        if constexpr (canSelfModulate)
        {
            return TR::isTargetModMatrixDepth(t);
        }
        else
        {
            return false;
        }
    }

    static constexpr bool canSupportsLag{details::has_supportsLag<TR>::value};
    bool supportsLag(typename TR::SourceIdentifier &s)
    {
        if constexpr (canSupportsLag)
        {
            return TR::supportsLag(s);
        }
        else
        {
            return false;
        }
    }

    static constexpr bool hasProvidesTargetRanges{details::has_providesTargetRanges<TR>::value};

    static constexpr bool supportsCurves{details::has_getCurveOperator<TR>::value};
};

template <typename ModMatrixTraits> struct FixedLengthRoutingTable : RoutingTable<ModMatrixTraits>
{
    using TR = ModMatrixTraits;
    using RT = RoutingTable<ModMatrixTraits>;
    static_assert(std::is_same<decltype(TR::FixedMatrixSize), const size_t>::value);

    std::array<typename RT::Routing, TR::FixedMatrixSize> routes{};

    // fixed API for changing the mod matrix in increasing completeness
    void updateDepthAt(size_t position, float depth)
    {
        assert(position < TR::FixedMatrixSize);
        routes[position].depth = depth;
    }

    void updateActiveAt(size_t position, bool act)
    {
        assert(position < TR::FixedMatrixSize);
        routes[position].active = act;
    }

    void updateRoutingAt(size_t position, const typename TR::SourceIdentifier &source,
                         const typename TR::TargetIdentifier &target, float depth)
    {
        assert(position < TR::FixedMatrixSize);
        routes[position].source = source;
        routes[position].sourceLagMS = 0;
        routes[position].sourceVia = std::nullopt;
        routes[position].sourceViaLagMS = 0;
        routes[position].curve = std::nullopt;
        routes[position].target = target;
        routes[position].depth = depth;
    }
    void updateRoutingAt(size_t position, const typename TR::SourceIdentifier &source,
                         const typename TR::SourceIdentifier &sourceVia,
                         const typename TR::CurveIdentifier &curve,
                         const typename TR::TargetIdentifier &target, float depth)
    {
        assert(position < TR::FixedMatrixSize);
        routes[position].source = source;
        routes[position].sourceLagMS = 0;
        routes[position].sourceVia = sourceVia;
        routes[position].sourceViaLagMS = 0;
        routes[position].curve = curve;
        routes[position].target = target;
        routes[position].depth = depth;
    }

    /**
     * Set the lag for the source. This gets snapped as static when
     * you call prepare.
     *
     * @param position The route index to update
     */
    void setSourceLagAt(size_t position, int16_t lagInMS, bool isExp = true)
    {
        routes[position].sourceLagMS = lagInMS;
        routes[position].sourceLagExp = isExp;
    }

    /**
     * Set the lag for the sourceVia. This gets snapped as static when
     * you call prepare.
     *
     * @param position The route index to update
     */
    void setSourceViaLagAt(size_t position, int16_t lagInMS, bool isExp = true)
    {
        routes[position].sourceViaLagMS = lagInMS;
        routes[position].sourceViaLagExp = isExp;
    }
};

template <typename ModMatrixTraits> struct FixedMatrix : ModMatrix<ModMatrixTraits>
{
    using TR = ModMatrixTraits;
    using PT = ModMatrix<ModMatrixTraits>;
    using RT = FixedLengthRoutingTable<ModMatrixTraits>;
    using RoutingTable = RT;

    std::array<float, TR::FixedMatrixSize> matrixOutputs{};

    struct RoutingValuePointers
    {
        bool *active{nullptr};
        float *source{nullptr}, *sourceVia{nullptr}, *depth{nullptr}, *target{nullptr};
        float depthScale{1.f};
        float maxVal{std::numeric_limits<float>::max()}, minVal{std::numeric_limits<float>::min()};
        std::function<float(float)> curveFn;

        ApplicationMode applicationMode;

        // We can handle the 'first time' by setting in prepare
        dsp::OnePoleLag<float, false> sourceLagExp, sourceViaLagExp;
        dsp::LinearLag<float, false> sourceLagLin, sourceViaLagLin;
        enum LagStyle
        {
            NONE = 0,
            EXPLAG = 1,
            LINLAG = 2
        };
        LagStyle sourceLagStyle{NONE}, sourceViaLagStyle{NONE};
    };
    std::array<RoutingValuePointers, TR::FixedMatrixSize> routingValuePointers{};

    std::unordered_map<typename TR::TargetIdentifier, bool> isOutputMapped;
    std::unordered_map<typename TR::TargetIdentifier, size_t> targetToOutputIndex;
    std::unordered_map<typename TR::SourceIdentifier, bool> isSourceUsed;

    void updateRoutingState(const RoutingTable &rt)
    {
        isOutputMapped.clear();
        isSourceUsed.clear();
        targetToOutputIndex.clear();
        size_t outIdx{0};
        for (auto &r : rt.routes)
        {
            if (!r.source.has_value() || !r.target.has_value())
                continue;

            isOutputMapped[*r.target] = true;
            isSourceUsed[*r.source] = true;
            if (r.sourceVia.has_value())
                isSourceUsed[*(r.sourceVia)] = true;

            if (targetToOutputIndex.find(*r.target) == targetToOutputIndex.end())
            {
                targetToOutputIndex.insert_or_assign(*r.target, outIdx);
                outIdx++;
            }
        }
    }

    void prepare(RT &rt, double sampleRate, int blockSize)
    {
        updateRoutingState(rt);

        int idx{0};
        std::unordered_set<typename TR::TargetIdentifier> depthMaps;
        for (auto &r : rt.routes)
        {
            if (!r.source.has_value() && !r.target.has_value())
                continue;

            auto &rv = routingValuePointers[idx];
            rv = RoutingValuePointers();
            idx++;
            if (this->sourceValues.find(*r.source) == this->sourceValues.end())
            {
                continue;
            }
            if (this->targetToOutputIndex.find(*r.target) == this->targetToOutputIndex.end())
            {
                continue;
            }

            rv.source = &this->sourceValues.at(*r.source);
            if (ModMatrix<TR>::supportsLag(*r.source) && r.sourceLagMS > 0)
            {
                if (r.sourceLagExp)
                {
                    rv.sourceLagExp.setRateInMilliseconds(r.sourceLagMS, sampleRate,
                                                          1.0 / blockSize);
                    rv.sourceLagExp.snapTo(*(rv.source));
                    rv.sourceLagStyle = RoutingValuePointers::EXPLAG;
                }
                else
                {
                    rv.sourceLagLin.setRateInMilliseconds(r.sourceLagMS, sampleRate,
                                                          1.0 / blockSize);
                    rv.sourceLagLin.snapTo(*(rv.source));
                    rv.sourceLagStyle = RoutingValuePointers::LINLAG;
                }
            }
            else
            {
                rv.sourceLagStyle = RoutingValuePointers::NONE;
            }
            if (r.sourceVia.has_value())
            {
                rv.sourceVia = &this->sourceValues.at(*(r.sourceVia));
                if (ModMatrix<TR>::supportsLag(*r.sourceVia) && r.sourceViaLagMS > 0)
                {
                    if (r.sourceViaLagExp)
                    {
                        rv.sourceViaLagExp.setRateInMilliseconds(r.sourceViaLagMS, sampleRate,
                                                                 1.0 / blockSize);
                        rv.sourceViaLagExp.snapTo(*(rv.sourceVia));
                        rv.sourceViaLagStyle = RoutingValuePointers::EXPLAG;
                    }
                    else
                    {
                        rv.sourceViaLagLin.setRateInMilliseconds(r.sourceViaLagMS, sampleRate,
                                                                 1.0 / blockSize);
                        rv.sourceViaLagLin.snapTo(*(rv.sourceVia));
                        rv.sourceViaLagStyle = RoutingValuePointers::LINLAG;
                    }
                }
                else
                {
                    rv.sourceViaLagStyle = RoutingValuePointers::NONE;
                }
            }

            if constexpr (ModMatrix<TR>::canSelfModulate)
            {
                if (TR::isTargetModMatrixDepth(*(r.target)))
                {
                    depthMaps.insert(*(r.target));
                }
            }

            rv.depthScale = 1.f;
            rv.depth = &r.depth;
            rv.active = &r.active;

            if constexpr (ModMatrix<TR>::supportsCurves)
            {
                if (r.curve.has_value())
                    rv.curveFn = TR::getCurveOperator(*(r.curve));
                else
                    rv.curveFn = nullptr;
            }

            rv.applicationMode = static_cast<ApplicationMode>(r.applicationMode);
            rv.target = &matrixOutputs[targetToOutputIndex.at(*r.target)];
        }

        if constexpr (ModMatrix<TR>::canSelfModulate)
        {
            for (auto m : depthMaps)
            {
                auto depthIndex = TR::getTargetModMatrixElement(m);
                assert(depthIndex < routingValuePointers.size());
                routingValuePointers[depthIndex].depth = &matrixOutputs[targetToOutputIndex.at(m)];
                this->baseValues.insert_or_assign(m, rt.routes[depthIndex].depth);
            }
        }
    }

    void process()
    {
        std::fill(matrixOutputs.begin(), matrixOutputs.end(), 0.f);

        for (const auto &[tgt, outIdx] : targetToOutputIndex)
        {
            matrixOutputs[outIdx] = this->baseValues.at(tgt);
        }
        for (auto &r : routingValuePointers)
        {
            if (!r.source || !r.target)
                continue;

            if (r.active && !(*r.active))
                continue;

            auto sourceVal = *(r.source);
            switch (r.sourceLagStyle)
            {
            case RoutingValuePointers::EXPLAG:
            {

                r.sourceLagExp.setTarget(sourceVal);
                r.sourceLagExp.process();
                sourceVal = r.sourceLagExp.v;
            }
            break;
            case RoutingValuePointers::LINLAG:
            {
                r.sourceLagLin.setTarget(sourceVal);
                r.sourceLagLin.process();
                sourceVal = r.sourceLagLin.v;
            }
            break;
            default:
                break;
            }

            float sourceViaVal{1.f};
            if (r.sourceVia)
            {
                sourceViaVal = *r.sourceVia;
                switch (r.sourceViaLagStyle)
                {
                case RoutingValuePointers::EXPLAG:
                {
                    r.sourceViaLagExp.setTarget(sourceViaVal);
                    r.sourceViaLagExp.process();
                    sourceViaVal = r.sourceViaLagExp.v;
                }
                break;
                case RoutingValuePointers::LINLAG:
                {
                    r.sourceViaLagLin.setTarget(sourceViaVal);
                    r.sourceViaLagLin.process();
                    sourceViaVal = r.sourceViaLagLin.v;
                }
                break;
                default:
                    break;
                }
            }

            auto offs = sourceVal * sourceViaVal;

            if constexpr (ModMatrix<TR>::supportsCurves)
            {
                if (r.curveFn)
                {
                    offs = r.curveFn(offs);
                }
            }
            switch (r.applicationMode)
            {
            case ApplicationMode::ADDITIVE:
                *(r.target) += *(r.depth) * r.depthScale * offs;
                break;
            case ApplicationMode::MULTIPLICATIVE:
            {
                // TODO - a bit more thoughtful about this clamp I bet
                offs = std::clamp(std::fabs(offs), 0.f, 1.f);
                auto dep = *r.depth;
                auto mulfac = 0.f;
                if (dep > 0)
                {
                    mulfac = dep * offs + (1 - dep);
                }
                else
                {
                    mulfac = 1 + dep * offs;
                }
                assert(mulfac >= 0 && mulfac <= 1.f);

                *(r.target) *= mulfac;
            }
            break;
            }
            if constexpr (ModMatrix<ModMatrixTraits>::hasProvidesTargetRanges)
            {
                *(r.target) = std::clamp(*(r.target), r.minVal, r.maxVal);
            }
        }
    }

    const float *getTargetValuePointer(const typename TR::TargetIdentifier &s) const
    {
        auto f = isOutputMapped.find(s);
        if (f == isOutputMapped.end() || !f->second)
        {
            return &this->baseValues.at(s);
        }
        else
        {
            return &matrixOutputs[targetToOutputIndex.at(s)];
        }
    }
    float getTargetValue(const typename TR::TargetIdentifier &s) const
    {
        auto p = getTargetValuePointer(s);
        if (p)
            return *p;
        return 0;
    }
};

} // namespace sst::basic_blocks::mod_matrix

#endif // SHORTCIRCUITXT_MODMATRIX_H
