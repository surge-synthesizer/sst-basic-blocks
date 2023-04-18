//
// Created by Paul Walker on 4/18/23.
//


#include "catch2.hpp"
#include "smoke_test_sse.h"

#include "sst/basic-blocks/mechanics/block-ops.h"

namespace mech = sst::basic_blocks::mechanics;
#include <iostream>

TEST_CASE("Clear and Copy", "[block]")
{
    SECTION( "64")
    {
        static constexpr int bs{64};
        float f alignas(16) [bs];
        float g alignas(16) [bs];
        float h alignas(16) [bs];

        for (int i=0; i<bs; ++i)
            f[i] = std::sin(i * 0.1);

        mech::copy_from_to<bs>(f, g);
        for (int i=0; i<bs; ++i)
            REQUIRE(f[i] == g[i]);

        mech::scale_by<bs>(f, g);
        for (int i=0; i<bs; ++i)
            REQUIRE(f[i] * f[i] == g[i]);

        mech::clear_block<bs>(g);
        for (int i=0; i<bs; ++i)
            REQUIRE(0 == g[i]);

        mech::accumulate_from_to<bs>(f, g);
        mech::accumulate_from_to<bs>(f, g);
        for (int i=0; i<bs; ++i)
            REQUIRE(2 * f[i] == g[i]);

        auto am = mech::blockAbsMax<bs>(g);
        int eq{0};
        for (int i=0; i<bs; ++i)
        {
            REQUIRE(std::fabs(g[i]) <= am);
            eq += std::fabs(g[i]) == am;
        }
        REQUIRE( eq > 0);
    }
}