//
// Created by Paul Walker on 4/19/23.
//

#include "catch2.hpp"
#include "smoke_test_sse.h"

#include "sst/basic-blocks/tables/DbToLinearProvider.h"

namespace tabl = sst::basic_blocks::tables;

TEST_CASE("DB to Linear", "[tables]")
{
    tabl::DbToLinearProvider dbt;
    dbt.init();

    for (float db = -192; db < 10; db += 0.0327)
    {
        INFO("Testing at " << db);
        REQUIRE(dbt.dbToLinear(db) == Approx(pow(10, db / 20.0)).margin(0.01));
    }
}