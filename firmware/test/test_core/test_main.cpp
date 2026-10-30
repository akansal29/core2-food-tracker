// Laptop unit tests for hardware-free logic. Run with:  pio test -e native
#include <unity.h>

#include "../../src/core/format.h"
#include "../../src/core/history.h"
#include "../../src/core/journal.h"
#include "../../src/core/nutrition.h"
#include "../../src/core/timeutil.h"
#include "../../src/core/vibe.h"

using namespace ft;

static int64_t at(int y, int mo, int d, int h, int mi, int s = 0) {
  CivilTime t{y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
  return toLocalSeconds(t);
}

void setUp() {}
void tearDown() {}

// ---- tracking day (guide Section 1) ----
void test_tracking_day_boundary() {
  TEST_ASSERT_EQUAL_UINT32(20261020, trackingDay(at(2026, 10, 20, 23, 30)));  // Tue 11:30 PM -> Tue
  TEST_ASSERT_EQUAL_UINT32(20261020, trackingDay(at(2026, 10, 21, 1, 30)));   // Wed 1:30 AM -> Tue
  TEST_ASSERT_EQUAL_UINT32(20261020, trackingDay(at(2026, 10, 21, 2, 59, 59)));
  TEST_ASSERT_EQUAL_UINT32(20261021, trackingDay(at(2026, 10, 21, 3, 0, 0)));  // Wed 3:00 AM -> Wed
}

void test_tracking_day_month_and_year_edges() {
  TEST_ASSERT_EQUAL_UINT32(20261031, trackingDay(at(2026, 11, 1, 2, 0)));
  TEST_ASSERT_EQUAL_UINT32(20261231, trackingDay(at(2027, 1, 1, 0, 15)));
  TEST_ASSERT_EQUAL_UINT32(20280228, trackingDay(at(2028, 2, 29, 1, 0)));  // leap day
  TEST_ASSERT_EQUAL_UINT32(20280229, trackingDay(at(2028, 2, 29, 3, 0)));
}

void test_civil_roundtrip_and_weekday() {
  int64_t s = at(2026, 10, 22, 0, 41, 12);
  CivilTime t = fromLocalSeconds(s);
  TEST_ASSERT_EQUAL_INT32(2026, t.year);
  TEST_ASSERT_EQUAL_UINT8(10, t.month);
  TEST_ASSERT_EQUAL_UINT8(22, t.day);
  TEST_ASSERT_EQUAL_UINT8(0, t.hour);
  TEST_ASSERT_EQUAL_UINT8(41, t.minute);
  TEST_ASSERT_EQUAL_UINT8(12, t.second);
  TEST_ASSERT_EQUAL_INT(3, weekday(s));  // Thursday
  TEST_ASSERT_TRUE(isPlausible(t));
  CivilTime bad{2000, 1, 1, 0, 0, 0};
  TEST_ASSERT_FALSE(isPlausible(bad));
}

// ---- journal (guide Section 8) ----
static Entry food(const char* id, int64_t ts, float kcal, float prot) {
  Entry e;
  e.id = id; e.ts = ts; e.kind = EntryKind::Food; e.foodId = "x"; e.name = "Thing | with bar";
  e.kcal = kcal; e.protein = prot;
  return e;
}

void test_journal_add_edit_delete_totals() {
  int64_t t1 = at(2026, 10, 21, 13, 0);
  int64_t late = at(2026, 10, 22, 1, 30);  // still the 21st's tracking day
  std::string j = encodeAdd(1, food("a", t1, 450, 12)) + "\n" + encodeAdd(2, food("b", late, 200, 8)) + "\n";
  Entry w; w.id = "w1"; w.ts = t1; w.kind = EntryKind::Water; w.waterOz = 8;
  j += encodeAdd(3, w) + "\n";
  Ledger L;
  size_t bad = 99;
  TEST_ASSERT_EQUAL(3, L.replay(j, &bad));
  TEST_ASSERT_EQUAL(0, bad);
  Totals t = L.totalsFor(20261021);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 650, t.kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 20, t.protein);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 8, t.waterOz);
  TEST_ASSERT_EQUAL_UINT32(2, t.foodCount);
  TEST_ASSERT_EQUAL_STRING("Thing / with bar", L.find("a")->name.c_str());

  Entry half = *L.find("a"); half.servings = 0.5f; half.kcal = 225; half.protein = 6;
  TEST_ASSERT_TRUE(L.applyLine(encodeEdit(4, half)));
  TEST_ASSERT_TRUE(L.applyLine(encodeDelete(5, "b")));
  t = L.totalsFor(20261021);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 225, t.kcal);
  TEST_ASSERT_TRUE(L.find("a")->edited);
  TEST_ASSERT_EQUAL_UINT32(5, L.lastSeq());
}

void test_journal_edit_time_moves_day() {
  Ledger L;
  TEST_ASSERT_TRUE(L.applyLine(encodeAdd(1, food("a", at(2026, 10, 21, 16, 0), 300, 10))));
  Entry moved = *L.find("a"); moved.ts = at(2026, 10, 21, 2, 0);  // previous tracking day
  TEST_ASSERT_TRUE(L.applyLine(encodeEdit(2, moved)));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0, L.totalsFor(20261021).kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 300, L.totalsFor(20261020).kcal);
}

void test_journal_rejects_torn_corrupt_and_duplicates() {
  std::string good = encodeAdd(1, food("a", at(2026, 10, 21, 12, 0), 100, 1));
  std::string corrupt = good; corrupt[corrupt.find("100.00")] = '9';
  std::string torn = encodeAdd(3, food("c", at(2026, 10, 21, 12, 0), 500, 5)).substr(0, 20);
  std::string j = good + "\n" + corrupt + "\n" + good + "\n" + torn;  // dup + no newline
  Ledger L;
  size_t bad = 0;
  TEST_ASSERT_EQUAL(1, L.replay(j, &bad));
  TEST_ASSERT_EQUAL(3, bad);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 100, L.totalsFor(20261021).kcal);
  TEST_ASSERT_FALSE(L.applyLine(encodeDelete(9, "nope")));
}

void test_entries_newest_first() {
  Ledger L;
  L.applyLine(encodeAdd(1, food("a", at(2026, 10, 21, 9, 0), 1, 0)));
  L.applyLine(encodeAdd(2, food("b", at(2026, 10, 21, 20, 0), 1, 0)));
  auto v = L.entriesFor(20261021);
  TEST_ASSERT_EQUAL(2, v.size());
  TEST_ASSERT_EQUAL_STRING("b", v[0]->id.c_str());
}

// ---- formatting (guide Section 7) ----
void test_format() {
  TEST_ASSERT_EQUAL_STRING("1,820 / 3,000 kcal", progressText(1820.4f, 3000, "kcal").c_str());
  TEST_ASSERT_EQUAL_STRING("3,250 / 3,000 kcal", progressText(3250, 3000, "kcal").c_str());
  TEST_ASSERT_EQUAL_STRING("999", withCommas(999).c_str());
  TEST_ASSERT_EQUAL_STRING("1,000,000", withCommas(1000000).c_str());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, progressFraction(3250, 3000));
  TEST_ASSERT_EQUAL_STRING("12:05 AM", clock12(0, 5).c_str());
  TEST_ASSERT_EQUAL_STRING("4:12 PM", clock12(16, 12).c_str());
}

// ---- vibration patterns (guide Section 8) ----
void test_vibe_patterns() {
  VibeSequencer v;
  v.start(VibePattern::Water, 1000);
  TEST_ASSERT_TRUE(v.isOn(1000));
  TEST_ASSERT_FALSE(v.isOn(1200));  // gap
  TEST_ASSERT_TRUE(v.isOn(1350));   // second pulse
  TEST_ASSERT_FALSE(v.isOn(1500));
  TEST_ASSERT_FALSE(v.active());
  v.start(VibePattern::Eating, 0);
  TEST_ASSERT_TRUE(v.isOn(599));
  TEST_ASSERT_FALSE(v.isOn(600));
}

// ---- food model (guide Sections 3 and 4) ----
static FoodDef plain(const char* id, const char* name, float kcal, float prot, uint16_t tags = 0) {
  FoodDef f;
  f.id = id; f.name = name; f.serving = "1"; f.kcal = kcal; f.protein = prot; f.tags = tags;
  return f;
}

static FoodDb sampleDb() {
  FoodDb db;
  db.add(plain("milk", "Chocolate milk", 190, 8, kDrinkable));
  db.add(plain("pb", "Peanut butter", 95, 3.5f));
  db.add(plain("old", "Old bar", 200, 10));
  FoodDef c = plain("combo", "Milk + PB", 999, 99, kComboTag);
  c.ingredients = {{"milk", 1}, {"pb", 1}};
  db.add(c);
  FoodDef& old = const_cast<FoodDef&>(*db.find("old"));
  old.archived = true;
  return db;
}

void test_combo_computed_and_categories() {
  FoodDb db = sampleDb();
  TEST_ASSERT_TRUE(db.computeCombos());
  const FoodDef* c = db.find("combo");
  TEST_ASSERT_FLOAT_WITHIN(0.01, 285, c->kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 11.5, c->protein);
  TEST_ASSERT_TRUE(categoryOf(*c) == Category::Meals);
  TEST_ASSERT_TRUE(categoryOf(*db.find("milk")) == Category::Drinks);
  TEST_ASSERT_TRUE(categoryOf(*db.find("pb")) == Category::Snacks);
  TEST_ASSERT_EQUAL(1, db.inCategory(Category::Snacks).size());  // archived "old" hidden

  Ingredients adj = {{"milk", 0.5f}, {"pb", 1}};
  Amount a = ingredientsTotal(db, adj);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 190, a.kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 7.5, a.protein);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 285, db.find("combo")->kcal);  // saved combo unchanged
}

void test_combo_bad_ingredient() {
  FoodDb db;
  FoodDef c = plain("c", "C", 0, 0);
  c.ingredients = {{"missing", 1}};
  db.add(c);
  std::vector<std::string> problems;
  TEST_ASSERT_FALSE(db.computeCombos(&problems));
  TEST_ASSERT_EQUAL(1, problems.size());
}

void test_servings() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.25, snapServings(0.1f));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.25, snapServings(1.3f));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 10, snapServings(12));
  TEST_ASSERT_EQUAL_STRING("1 serving", servingsLabel(1).c_str());
  TEST_ASSERT_EQUAL_STRING("0.5 servings", servingsLabel(0.5f).c_str());
  TEST_ASSERT_EQUAL_STRING("1.25 servings", servingsLabel(1.25f).c_str());
  TEST_ASSERT_EQUAL_STRING("2 servings", servingsLabel(2).c_str());
  Amount a = scale(plain("x", "X", 190, 8), 0.75f);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 142.5, a.kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 6, a.protein);
}

void test_recents() {
  FoodDb db = sampleDb();
  Ledger L;
  auto add = [&](uint32_t seq, const char* id, const char* food, int64_t ts, EntryKind k) {
    Entry e; e.id = id; e.ts = ts; e.kind = k; e.foodId = food; e.name = food;
    L.applyLine(encodeAdd(seq, e));
  };
  int64_t t = at(2026, 10, 21, 9, 0);
  add(1, "a", "milk", t, EntryKind::Food);
  add(2, "b", "pb", t + 60, EntryKind::Food);
  add(3, "c", "milk", t + 120, EntryKind::Food);
  add(4, "d", "", t + 180, EntryKind::Water);
  add(5, "e", "old", t + 240, EntryKind::Food);   // archived: skipped
  add(6, "f", "", t + 300, EntryKind::Custom);    // unsaved custom: skipped
  add(7, "g", "combo", t + 360, EntryKind::Combo);
  auto r = recentFoodIds(L, db, 5);
  TEST_ASSERT_EQUAL(3, r.size());
  TEST_ASSERT_EQUAL_STRING("combo", r[0].c_str());
  TEST_ASSERT_EQUAL_STRING("milk", r[1].c_str());
  TEST_ASSERT_EQUAL_STRING("pb", r[2].c_str());
  TEST_ASSERT_EQUAL(2, recentFoodIds(L, db, 2).size());
  TEST_ASSERT_EQUAL(0, parseTag("crunchy"));
  TEST_ASSERT_EQUAL(kDrinkable, parseTag("drinkable"));
}

// ---- history (guide Sections 4 and 5) ----
void test_logged_days_and_labels() {
  Ledger L;
  L.applyLine(encodeAdd(1, food("a", at(2026, 10, 28, 12, 0), 100, 1)));
  L.applyLine(encodeAdd(2, food("b", at(2026, 10, 30, 1, 0), 100, 1)));  // still the 29th
  L.applyLine(encodeAdd(3, food("c", at(2026, 10, 30, 9, 0), 100, 1)));
  L.applyLine(encodeAdd(4, food("d", at(2026, 10, 28, 18, 0), 100, 1)));
  auto days = loggedDays(L);
  TEST_ASSERT_EQUAL(3, days.size());
  TEST_ASSERT_EQUAL_UINT32(20261030, days[0]);
  TEST_ASSERT_EQUAL_UINT32(20261029, days[1]);
  TEST_ASSERT_EQUAL_UINT32(20261028, days[2]);
  TEST_ASSERT_EQUAL_STRING("Thu Oct 29", dayLabel(20261029).c_str());
  TEST_ASSERT_EQUAL_STRING("Fri Jan 1", dayLabel(20270101).c_str());
}

void test_rescale_uses_snapshot() {
  Entry e = food("a", 0, 190, 8);
  e.servings = 1;
  Entry half = rescaleEntry(e, 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 95, half.kcal);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 4, half.protein);
  Entry back = rescaleEntry(half, 2);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 380, back.kcal);
  Entry w; w.kind = EntryKind::Water; w.waterOz = 8;
  TEST_ASSERT_FLOAT_WITHIN(0.01, 8, rescaleEntry(w, 3).waterOz);
}

void test_shift_time_and_remaining() {
  int64_t now = at(2026, 10, 30, 18, 0);
  TEST_ASSERT_TRUE(shiftTime(now - 600, 3600, now) == now);   // never into the future
  TEST_ASSERT_TRUE(shiftTime(now, -900, now) == now - 900);
  TEST_ASSERT_EQUAL_STRING("1,180 kcal and 46 g to go", remainingText(1820, 3000, 54, 100).c_str());
  TEST_ASSERT_EQUAL_STRING("Calories reached, 12 g protein to go", remainingText(3100, 3000, 88, 100).c_str());
  TEST_ASSERT_EQUAL_STRING("500 kcal to go, protein reached", remainingText(2500, 3000, 120, 100).c_str());
  TEST_ASSERT_EQUAL_STRING("Both targets reached", remainingText(3000, 3000, 100, 100).c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_tracking_day_boundary);
  RUN_TEST(test_tracking_day_month_and_year_edges);
  RUN_TEST(test_civil_roundtrip_and_weekday);
  RUN_TEST(test_journal_add_edit_delete_totals);
  RUN_TEST(test_journal_edit_time_moves_day);
  RUN_TEST(test_journal_rejects_torn_corrupt_and_duplicates);
  RUN_TEST(test_entries_newest_first);
  RUN_TEST(test_format);
  RUN_TEST(test_vibe_patterns);
  RUN_TEST(test_combo_computed_and_categories);
  RUN_TEST(test_combo_bad_ingredient);
  RUN_TEST(test_servings);
  RUN_TEST(test_recents);
  RUN_TEST(test_logged_days_and_labels);
  RUN_TEST(test_rescale_uses_snapshot);
  RUN_TEST(test_shift_time_and_remaining);
  return UNITY_END();
}
