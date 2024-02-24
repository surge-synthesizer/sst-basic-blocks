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

#include "sst/basic-blocks/mod-matrix/ModMatrix.h"
#include <cassert>
#include "catch2.hpp"

using namespace sst::basic_blocks::mod_matrix;

struct BldConfig
{
    using SourceIdentifier = int;
    using TargetIdentifier = int;
    using CurveIdentifier = int;

    using RoutingExtraPayload = int;

    static constexpr bool IsFixedMatrix{false};
};
TEST_CASE("Construct", "[mod-matrix]") { ModMatrix<BldConfig> m; }

struct Config
{
    struct SourceIdentifier
    {
        enum SI
        {
            FOO,
            BAR,
            HOOTIE
        } src{FOO};
        int index0{0};
        int index1{0};

        bool operator==(const SourceIdentifier &other) const
        {
            return src == other.src && index0 == other.index0 && index1 == other.index1;
        }
    };

    struct TargetIdentifier
    {
        int baz{0};
        uint32_t nm{};
        int16_t depthPosition{-1};

        bool operator==(const TargetIdentifier &other) const
        {
            return baz == other.baz && nm == other.nm && depthPosition == other.depthPosition;
        }
    };

    using CurveIdentifier = int;

    static bool isTargetModMatrixDepth(const TargetIdentifier &t) { return t.depthPosition >= 0; }
    static size_t getTargetModMatrixElement(const TargetIdentifier &t)
    {
        assert(isTargetModMatrixDepth(t));
        return (size_t)t.depthPosition;
    }

    using RoutingExtraPayload = int;

    static constexpr bool IsFixedMatrix{true};
    static constexpr size_t FixedMatrixSize{16};
};

template <> struct std::hash<Config::SourceIdentifier>
{
    std::size_t operator()(const Config::SourceIdentifier &s) const noexcept
    {
        auto h1 = std::hash<int>{}((int)s.src);
        auto h2 = std::hash<int>{}((int)s.index0);
        auto h3 = std::hash<int>{}((int)s.index1);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

template <> struct std::hash<Config::TargetIdentifier>
{
    std::size_t operator()(const Config::TargetIdentifier &s) const noexcept
    {
        auto h1 = std::hash<int>{}((int)s.baz);
        auto h2 = std::hash<uint32_t>{}((int)s.nm);

        return h1 ^ (h2 << 1);
    }
};

TEST_CASE("Configure and Bind", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto fooS = Config::SourceIdentifier{Config::SourceIdentifier::SI::FOO};

    auto tg3T = Config::TargetIdentifier{3};
    auto tg3PT = Config::TargetIdentifier{3, 'facd'};

    float barSVal{1.1}, fooSVal{2.3};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(fooS, fooSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);
    m.bindTargetBaseValue(tg3PT, t3PV);

    m.prepare(rt);

    m.process();

    INFO("Absent modulation the base values should just shine through")
    REQUIRE(m.getTargetValue(tg3T) == t3V);
    REQUIRE(m.getTargetValue(tg3PT) == t3PV);

    t3V = 0.5;
    t3PV = -0.1;

    m.process();

    REQUIRE(m.getTargetValue(tg3T) == t3V);
    REQUIRE(m.getTargetValue(tg3PT) == t3PV);
}

TEST_CASE("Configure Bind and Route", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto fooS = Config::SourceIdentifier{Config::SourceIdentifier::SI::FOO};

    auto tg3T = Config::TargetIdentifier{3};
    auto tg3PT = Config::TargetIdentifier{3, 'facd'};

    float barSVal{1.1}, fooSVal{2.3};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(fooS, fooSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);
    m.bindTargetBaseValue(tg3PT, t3PV);

    rt.updateRoutingAt(0, barS, tg3T, 0.5);
    rt.updateRoutingAt(1, fooS, tg3PT, -0.5);

    m.prepare(rt);

    m.process();

    INFO("Absent modulation the base values should just shine through")
    REQUIRE(m.getTargetValue(tg3T) == Approx(t3V + 0.5 * barSVal).margin(1e-5));
    REQUIRE(m.getTargetValue(tg3PT) == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));

    t3V = 0.5;
    t3PV = -0.1;

    m.process();

    REQUIRE(m.getTargetValue(tg3T) == Approx(t3V + 0.5 * barSVal).margin(1e-5));
    REQUIRE(m.getTargetValue(tg3PT) == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));
}

TEST_CASE("Configure Bind and Route Pointer Get API", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto fooS = Config::SourceIdentifier{Config::SourceIdentifier::SI::FOO};

    auto tg3T = Config::TargetIdentifier{3};
    auto tg3PT = Config::TargetIdentifier{3, 'facd'};

    float barSVal{1.1}, fooSVal{2.3};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(fooS, fooSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);
    m.bindTargetBaseValue(tg3PT, t3PV);

    rt.updateRoutingAt(0, barS, tg3T, 0.5);
    rt.updateRoutingAt(1, fooS, tg3PT, -0.5);

    m.prepare(rt);

    m.process();

    auto t3P = m.getTargetValuePointer(tg3T);
    auto t3PP = m.getTargetValuePointer(tg3PT);

    INFO("Absent modulation the base values should just shine through")
    REQUIRE(t3P);
    REQUIRE(t3PP);
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal).margin(1e-5));
    REQUIRE(*t3PP == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));

    t3V = 0.5;
    t3PV = -0.1;

    m.process();

    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal).margin(1e-5));
    REQUIRE(*t3PP == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));
}

TEST_CASE("Modulate Depth", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto depS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 17, 3};

    auto tgDepth = Config::TargetIdentifier{1, 'fowq', 1};
    auto tg3T = Config::TargetIdentifier{3};

    float barSVal{1.1}, depVal{0.f};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(depS, depVal);

    float t3V{0.2};
    m.bindTargetBaseValue(tg3T, t3V);

    rt.updateRoutingAt(0, depS, tgDepth, 0.2);
    rt.updateRoutingAt(1, barS, tg3T, 0.5);

    m.prepare(rt);

    m.process();

    auto t3P = m.getTargetValuePointer(tg3T);
    REQUIRE(*(m.routingValuePointers[1].depth) == depVal * 0.2 + 0.5);

    REQUIRE(t3P);
    REQUIRE(*t3P == Approx(t3V + (0.5 + 0.2 * depVal) * barSVal).margin(1e-5));

    t3V = 0.5;
    depVal = 0.12;

    m.process();
    REQUIRE(*t3P == Approx(t3V + (0.5 + 0.2 * depVal) * barSVal).margin(1e-5));
}

TEST_CASE("Routing Activation", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto fooS = Config::SourceIdentifier{Config::SourceIdentifier::SI::FOO};

    auto tg3T = Config::TargetIdentifier{3};
    auto tg3PT = Config::TargetIdentifier{3, 'facd'};

    float barSVal{1.1}, fooSVal{2.3};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(fooS, fooSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);
    m.bindTargetBaseValue(tg3PT, t3PV);

    rt.updateRoutingAt(0, barS, tg3T, 0.5);
    rt.updateRoutingAt(1, fooS, tg3PT, -0.5);

    m.prepare(rt);

    m.process();

    auto t3P = m.getTargetValuePointer(tg3T);
    auto t3PP = m.getTargetValuePointer(tg3PT);

    INFO("Absent modulation the base values should just shine through")
    REQUIRE(t3P);
    REQUIRE(t3PP);
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal).margin(1e-5));
    REQUIRE(*t3PP == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));

    rt.routes[0].active = false;
    m.process();

    REQUIRE(*t3P == Approx(t3V).margin(1e-5));
    REQUIRE(*t3PP == Approx(t3PV - 0.5 * fooSVal).margin(1e-5));
}

TEST_CASE("Routing Via", "[mod-matrix]")
{
    FixedMatrix<Config> m;
    FixedMatrix<Config>::RoutingTable rt;

    auto barS = Config::SourceIdentifier{Config::SourceIdentifier::SI::BAR, 2, 3};
    auto fooS = Config::SourceIdentifier{Config::SourceIdentifier::SI::FOO};

    auto tg3T = Config::TargetIdentifier{3};

    float barSVal{1.1}, fooSVal{2.3};
    m.bindSourceValue(barS, barSVal);
    m.bindSourceValue(fooS, fooSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);

    // bind with a 'via'
    rt.updateRoutingAt(0, barS, fooS, {}, tg3T, 0.5);

    m.prepare(rt);

    m.process();

    auto t3P = m.getTargetValuePointer(tg3T);

    INFO("Absent modulation the base values should just shine through")
    REQUIRE(t3P);
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal * fooSVal).margin(1e-5));

    fooSVal = 0.48;
    m.process();
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal * fooSVal).margin(1e-5));

    barSVal = 0.298;
    m.process();
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal * fooSVal).margin(1e-5));
}

struct CurveConfig
{
    using SourceIdentifier = int;
    using TargetIdentifier = int;
    using CurveIdentifier = int;

    using RoutingExtraPayload = int;

    static constexpr bool IsFixedMatrix{true};
    static constexpr size_t FixedMatrixSize{16};

    static std::function<float(float)> getCurveOperator(CurveIdentifier id)
    {
        switch (id)
        {
        case 2:
            return [](auto x) { return std::sin(x); };
        case 1:
            return [](auto x) { return x * x * x; };
        }
        return [](auto x) { return x; };
    }
};

TEST_CASE("WithCurves", "[mod-matrix]")
{
    FixedMatrix<CurveConfig> m;
    FixedMatrix<CurveConfig>::RoutingTable rt;

    auto barS = CurveConfig::SourceIdentifier{7};

    auto tg3T = CurveConfig::TargetIdentifier{3};

    float barSVal{1.1};
    m.bindSourceValue(barS, barSVal);

    float t3V{0.2}, t3PV{0.3};
    m.bindTargetBaseValue(tg3T, t3V);

    // bind with a 'via'
    rt.updateRoutingAt(0, barS, tg3T, 0.5);
    rt.routes[0].curve = 0;

    m.prepare(rt);
    m.process();

    auto t3P = m.getTargetValuePointer(tg3T);

    REQUIRE(t3P);
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal).margin(1e-5));

    rt.routes[0].curve = 1;
    m.prepare(rt);
    m.process();

    REQUIRE(t3P);
    REQUIRE(*t3P == Approx(t3V + 0.5 * barSVal * barSVal * barSVal).margin(1e-5));

    rt.routes[0].curve = 2;
    m.prepare(rt);
    m.process();

    REQUIRE(t3P);
    REQUIRE(*t3P == Approx(t3V + 0.5 * std::sin(barSVal)).margin(1e-5));
}
