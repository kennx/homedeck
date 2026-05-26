#include <unity.h>

#include <vector>

#include "view_manager.h"

namespace {

struct Fixture {
  std::vector<std::string> renderedViews;
  bool buttonClicked = false;

  homedeck::ViewManagerDeps deps() {
    homedeck::ViewManagerDeps deps{};
    deps.renderAlmanac = [this]() { renderedViews.push_back("almanac"); };
    deps.renderCalendar = [this]() { renderedViews.push_back("calendar"); };
    deps.wasCalendarButtonClicked = [this]() { return buttonClicked; };
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
  TEST_ASSERT_TRUE(vm.viewSwitched());
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.renderedViews.size()));
  TEST_ASSERT_EQUAL_STRING("almanac", f.renderedViews[0].c_str());
}

void test_view_manager_switches_to_calendar_on_button_click() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  f.renderedViews.clear();

  f.buttonClicked = true;
  vm.update();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Calendar, vm.currentView());
  TEST_ASSERT_TRUE(vm.viewSwitched());
  TEST_ASSERT_EQUAL(1, static_cast<int>(f.renderedViews.size()));
  TEST_ASSERT_EQUAL_STRING("calendar", f.renderedViews[0].c_str());
}

void test_view_manager_switches_back_to_almanac_on_second_click() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  f.renderedViews.clear();

  f.buttonClicked = true;
  vm.update();
  f.buttonClicked = false;
  vm.update();

  f.buttonClicked = true;
  vm.update();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_TRUE(vm.viewSwitched());
  TEST_ASSERT_EQUAL_STRING("almanac", f.renderedViews.back().c_str());
}

void test_view_manager_does_not_switch_when_button_not_clicked() {
  Fixture f{};
  homedeck::ViewManager vm{f.deps()};
  vm.begin();
  f.renderedViews.clear();

  f.buttonClicked = false;
  vm.update();

  TEST_ASSERT_EQUAL(homedeck::SystemView::Almanac, vm.currentView());
  TEST_ASSERT_FALSE(vm.viewSwitched());
  TEST_ASSERT_EQUAL(0, static_cast<int>(f.renderedViews.size()));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_view_manager_begins_with_almanac);
  RUN_TEST(test_view_manager_switches_to_calendar_on_button_click);
  RUN_TEST(test_view_manager_switches_back_to_almanac_on_second_click);
  RUN_TEST(test_view_manager_does_not_switch_when_button_not_clicked);
  return UNITY_END();
}
