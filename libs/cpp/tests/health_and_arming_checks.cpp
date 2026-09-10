#include "parse/health_and_arming_checks.h"

#include <events_generated.h>
#include <gtest/gtest.h>

#include <cassert>
#include <string>

#include "parse/parser.h"

namespace
{

std::string getTestJsonFile()
{
    // Get directory of this source file
    const std::string this_file = __FILE__;
    const size_t last_slash_idx = this_file.rfind('/');
    assert(std::string::npos != last_slash_idx);
    const std::string this_directory = this_file.substr(0, last_slash_idx);

    return this_directory + "/../../test.json";
}

using health_component_t = events::test2::enums::health_component_t;
using navigation_mode_group_t = events::test2::enums::navigation_mode_group_t;

constexpr uint8_t gps_index = 3;  // health_component_t::gps == 1 << 3

/**
 * Feed one complete batch (arming check summary, one failing check, health summary)
 * and return whether the results got updated.
 */
bool feedBatch(events::parser::Parser& parser, events::HealthAndArmingChecks& checks, uint8_t chunk_idx)
{
    const auto summary = events::test2::create_arming_check_summary<events::EventType>(
        events::Log::Protocol, chunk_idx, health_component_t::gps, health_component_t::none,
        navigation_mode_group_t::manual | navigation_mode_group_t::altctl, navigation_mode_group_t::manual);
    const auto check = events::test2::create_check_modes_global_pos<events::EventType>(
        events::Log::Error, navigation_mode_group_t::posctl | navigation_mode_group_t::mission, gps_index);
    const auto health = events::test2::create_health_summary_test<events::EventType>(
        events::Log::Protocol, chunk_idx, health_component_t::gps, health_component_t::gps, health_component_t::none);

    bool updated = false;
    for (const auto& event : {summary, check, health}) {
        auto parsed = parser.parse(event);
        if (!parsed) {
            return false;
        }
        updated = checks.handleEvent(*parsed);
    }
    return updated;
}

}  // namespace

TEST(HealthAndArmingChecks, ChecksLinkToHealthComponents)
{
    events::parser::Parser parser;
    ASSERT_TRUE(parser.loadDefinitionsFile(getTestJsonFile()));
    events::HealthAndArmingChecks checks;

    ASSERT_TRUE(feedBatch(parser, checks, 0));

    const auto& results = checks.results();
    ASSERT_EQ(results.checks().size(), 1u);
    const auto& check = results.checks()[0];
    EXPECT_EQ(check.type, events::HealthAndArmingChecks::CheckType::ArmingCheck);
    EXPECT_EQ(check.affected_health_component_index, gps_index);

    // The pointer must refer to the entry in the results' own health component map
    const auto& components = results.healthComponents().health_components;
    ASSERT_NE(check.health_component, nullptr);
    EXPECT_EQ(check.health_component->name, "gps");
    EXPECT_EQ(check.health_component, &components.at("gps"));
    EXPECT_TRUE(check.health_component->arming_check.error);
    EXPECT_TRUE(check.health_component->health.is_present);
    EXPECT_TRUE(check.health_component->health.error);
}

TEST(HealthAndArmingChecks, CopiedResultsAreSelfContained)
{
    events::parser::Parser parser;
    ASSERT_TRUE(parser.loadDefinitionsFile(getTestJsonFile()));
    events::HealthAndArmingChecks checks;

    ASSERT_TRUE(feedBatch(parser, checks, 0));
    const events::HealthAndArmingChecks::Results copy = checks.results();

    // A copy links its checks to its own map, not to the original's
    ASSERT_EQ(copy.checks().size(), 1u);
    ASSERT_NE(copy.checks()[0].health_component, nullptr);
    EXPECT_EQ(copy.checks()[0].health_component, &copy.healthComponents().health_components.at("gps"));
    EXPECT_NE(copy.checks()[0].health_component, checks.results().checks()[0].health_component);

    // ...and stays valid after the original results are rebuilt by the next batch
    ASSERT_TRUE(feedBatch(parser, checks, 0));
    EXPECT_EQ(copy.checks()[0].health_component, &copy.healthComponents().health_components.at("gps"));
    EXPECT_EQ(copy.checks()[0].health_component->name, "gps");

    events::HealthAndArmingChecks::Results assigned;
    assigned = copy;
    EXPECT_EQ(assigned.checks()[0].health_component, &assigned.healthComponents().health_components.at("gps"));

    const events::HealthAndArmingChecks::Results moved = std::move(assigned);
    EXPECT_EQ(moved.checks()[0].health_component, &moved.healthComponents().health_components.at("gps"));
}

TEST(HealthAndArmingChecks, ChecksWithoutComponentHaveNoPointer)
{
    events::parser::Parser parser;
    ASSERT_TRUE(parser.loadDefinitionsFile(getTestJsonFile()));
    events::HealthAndArmingChecks checks;

    const auto summary = events::test2::create_arming_check_summary<events::EventType>(
        events::Log::Protocol, 0, health_component_t::none, health_component_t::none, navigation_mode_group_t::manual,
        navigation_mode_group_t::manual);
    const auto check = events::test2::create_check_modes_local_alt<events::EventType>(
        events::Log::Error, navigation_mode_group_t::posctl, 0);  // component index 0 == none
    const auto health = events::test2::create_health_summary_test<events::EventType>(
        events::Log::Protocol, 0, health_component_t::none, health_component_t::none, health_component_t::none);

    for (const auto& event : {summary, check, health}) {
        auto parsed = parser.parse(event);
        ASSERT_TRUE(parsed);
        checks.handleEvent(*parsed);
    }

    ASSERT_EQ(checks.results().checks().size(), 1u);
    EXPECT_EQ(checks.results().checks()[0].health_component, nullptr);
}
