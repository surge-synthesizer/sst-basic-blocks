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

#include <iostream>
#include "ModMatrixDetails.h"

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

        float depth{0};

        typename TR::RoutingExtraPayload extraPayload;
    };
};

template <typename ModMatrixTraits>
struct ModMatrix : details::CheckModMatrixConstraints<ModMatrixTraits>
{
    using TR = ModMatrixTraits;

    std::unordered_map<typename TR::TargetIdentifier, float &> baseValues;
    void bindTargetBaseValue(const typename TR::TargetIdentifier &t, float &f)
    {
        baseValues.insert_or_assign(t, f);
    }

    std::unordered_map<typename TR::SourceIdentifier, float &> sourceValues;
    void bindSourceValue(const typename TR::SourceIdentifier &s, float &f)
    {
        sourceValues.insert_or_assign(s, f);
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

    static constexpr bool supportsCurves{details::has_getCurveOperator<TR>::value};
};

template <typename ModMatrixTraits> struct FixedLengthRoutingTable : RoutingTable<ModMatrixTraits>
{
    using TR = ModMatrixTraits;
    using RT = RoutingTable<ModMatrixTraits>;
    static_assert(std::is_same<decltype(TR::FixedMatrixSize), const size_t>::value);

    std::array<typename RT::Routing, TR::FixedMatrixSize> routes{};
    std::unordered_map<typename TR::TargetIdentifier, bool> isOutputMapped;
    std::unordered_map<typename TR::TargetIdentifier, size_t> targetToOutputIndex;
    std::unordered_map<typename TR::SourceIdentifier, bool> isSourceUsed;

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
        routes[position].target = target;
        routes[position].depth = depth;

        updateRoutingState();
    }
    void updateRoutingAt(size_t position, const typename TR::SourceIdentifier &source,
                         const typename TR::SourceIdentifier &sourceVia,
                         const typename TR::CurveIdentifier &curve,
                         const typename TR::TargetIdentifier &target, float depth)
    {
        assert(position < TR::FixedMatrixSize);
        routes[position].source = source;
        routes[position].sourceVia = sourceVia;
        routes[position].curve = curve;
        routes[position].target = target;
        routes[position].depth = depth;

        updateRoutingState();
    }

    void updateRoutingState()
    {
        isOutputMapped.clear();
        isSourceUsed.clear();
        targetToOutputIndex.clear();
        size_t outIdx{0};
        for (auto &r : routes)
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
};

template <typename ModMatrixTraits> struct FixedMatrix : ModMatrix<ModMatrixTraits>
{
    using TR = ModMatrixTraits;
    using PT = ModMatrix<ModMatrixTraits>;
    using RT = FixedLengthRoutingTable<ModMatrixTraits>;
    using RoutingTable = RT;

    RoutingTable routingTable;

    std::array<float, TR::FixedMatrixSize> matrixOutputs{};

    struct RoutingValuePointers
    {
        bool *active{nullptr};
        float *source{nullptr}, *sourceVia{nullptr}, *depth{nullptr}, *target{nullptr};
        float depthScale{1.f};
        std::function<float(float)> curveFn;
    };
    std::array<RoutingValuePointers, TR::FixedMatrixSize> routingValuePointers{};

    void prepare(RT rt)
    {
        this->routingTable = rt;
        this->routingTable.updateRoutingState();

        int idx{0};
        std::unordered_set<typename TR::TargetIdentifier> depthMaps;
        for (auto &r : routingTable.routes)
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
            if (this->routingTable.targetToOutputIndex.find(*r.target) ==
                this->routingTable.targetToOutputIndex.end())
            {
                continue;
            }

            rv.source = &this->sourceValues.at(*r.source);
            if (r.sourceVia.has_value())
                rv.sourceVia = &this->sourceValues.at(*(r.sourceVia));

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

            rv.target = &matrixOutputs[routingTable.targetToOutputIndex.at(*r.target)];
        }

        if constexpr (ModMatrix<TR>::canSelfModulate)
        {
            for (auto m : depthMaps)
            {
                auto depthIndex = TR::getTargetModMatrixElement(m);
                assert(depthIndex < routingValuePointers.size());
                routingValuePointers[depthIndex].depth =
                    &matrixOutputs[routingTable.targetToOutputIndex.at(m)];
                this->baseValues.insert_or_assign(m, this->routingTable.routes[depthIndex].depth);
            }
        }
    }

    void process()
    {
        std::fill(matrixOutputs.begin(), matrixOutputs.end(), 0.f);

        for (const auto &[tgt, outIdx] : routingTable.targetToOutputIndex)
        {
            matrixOutputs[outIdx] = this->baseValues.at(tgt);
        }
        for (auto r : routingValuePointers)
        {
            if (r.active && !(*r.active))
                continue;

            if (!r.source || !r.target)
                continue;

            float sourceViaVal{1.f};
            if (r.sourceVia)
                sourceViaVal = *r.sourceVia;

            if constexpr (ModMatrix<TR>::supportsCurves)
            {
                auto offs = *(r.source) * sourceViaVal;
                if (r.curveFn)
                    *(r.target) += *(r.depth) * r.depthScale * r.curveFn(offs);
                else
                    *(r.target) += *(r.depth) * r.depthScale * offs;
            }
            else
            {
                *(r.target) += *(r.source) * *(r.depth) * r.depthScale * sourceViaVal;
            }
        }
    }

    const float *getTargetValuePointer(const typename TR::TargetIdentifier &s) const
    {
        auto f = routingTable.isOutputMapped.find(s);
        if (f == routingTable.isOutputMapped.end() || !f->second)
        {
            return &this->baseValues.at(s);
        }
        else
        {
            return &matrixOutputs[routingTable.targetToOutputIndex.at(s)];
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
