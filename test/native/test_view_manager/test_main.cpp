#include <unity.h>

#include <vector>

#include "app/view_manager.h"

namespace {

struct Fixture {
  std::vector<std::string> renderedViews;

  homedeck::ViewManagerDeps deps() {
    homedeck::ViewManagerDeps deps{};
    deps.renderAlmanac = [this]() { renderedViews.push_back("almanac"); };
    deps.renderCalendar = [this]() { renderedViews.push_back("calendar"); };
    deps.renderCountdown = [this]() { renderedViews.push_back("countdown"); };
    return deps;
  }
};

}  // namespace

void setUp() {
}

void tearDown() {
}

void test_view_manager_begins_with_almanac() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};

  vm.begin();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.renderedViews.size()));
  TEST_ASSERT_EQUAL_STRING("almanac", f.renderedViews[0].c_str());
}

void test_view_manager_switch_to_next_view_from_almanac() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  f.renderedViews.clear();

  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, vm.currentView());
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.renderedViews.size()));
  TEST_ASSERT_EQUAL_STRING("calendar", f.renderedViews[0].c_str());
}

void test_view_manager_switch_to_next_view_from_calendar() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  vm.switchToNextView();
  f.renderedViews.clear();

  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Countdown, vm.currentView());
  TEST_ASSERT_EQUAL_STRING("countdown", f.renderedViews[0].c_str());
}

void test_view_manager_switch_to_next_view_from_countdown() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  vm.switchToNextView();
  vm.switchToNextView();
  f.renderedViews.clear();

  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_EQUAL_STRING("almanac", f.renderedViews[0].c_str());
}

void test_view_manager_switch_to_next_view_cycles() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();

  vm.switchToNextView();
  vm.switchToNextView();
  vm.switchToNextView();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
}

void test_view_manager_does_not_switch_without_call() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  f.renderedViews.clear();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.renderedViews.size()));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_view_manager_begins_with_almanac);
  RUN_TEST(test_view_manager_switch_to_next_view_from_almanac);
  RUN_TEST(test_view_manager_switch_to_next_view_from_calendar);
  RUN_TEST(test_view_manager_switch_to_next_view_from_countdown);
  RUN_TEST(test_view_manager_switch_to_next_view_cycles);
  RUN_TEST(test_view_manager_does_not_switch_without_call);
  return UNITY_END();
}
