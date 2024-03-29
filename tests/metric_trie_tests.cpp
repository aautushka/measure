/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include "measure/measure.h"
#include <gtest/gtest.h>

struct metric_trie_test : ::testing::Test {
  using trie_t = measure::trie<int, int>;
  trie_t trie;
  trie_t lhs;
  trie_t rhs;
};

TEST_F(metric_trie_test, adds_node) {
  trie.down(11) = 123;
  EXPECT_EQ(123, trie.get());
}

TEST_F(metric_trie_test, adds_second_node) {
  trie.down(11) = 123;
  trie.down(11) = 456;

  EXPECT_EQ(456, trie.get());
}

TEST_F(metric_trie_test, goes_one_node_up) {
  trie.down(11) = 123;
  trie.down(22) = 456;

  EXPECT_EQ(456, trie.up());
  EXPECT_EQ(123, trie.get());
}

TEST_F(metric_trie_test, builds_stack) {
  trie.down(1) = 1;
  trie.down(2) = 2;
  trie.down(3) = 3;

  EXPECT_EQ(1, trie.at({1}));
  EXPECT_EQ(2, trie.at({1, 2}));
  EXPECT_EQ(3, trie.at({1, 2, 3}));
}

TEST_F(metric_trie_test, never_clears_data) {
  trie.down(1) = 1;
  trie.down(2) = 2;
  trie.down(3) = 3;

  trie.up();
  trie.up();
  trie.up();

  EXPECT_EQ(1, trie.at({1}));
  EXPECT_EQ(2, trie.at({1, 2}));
  EXPECT_EQ(3, trie.at({1, 2, 3}));
}

TEST_F(metric_trie_test, adds_sibling_node) {
  trie.down(1) = 1;

  trie.down(2) = 2;
  trie.up();

  trie.down(3) = 3;
  trie.up();

  trie.down(4) = 4;
  trie.up();

  EXPECT_EQ(2, trie.at({1, 2}));
  EXPECT_EQ(3, trie.at({1, 3}));
  EXPECT_EQ(4, trie.at({1, 4}));
}

TEST_F(metric_trie_test, drills_down_the_stack) {
  trie.create({1, 2, 3, 4}) = 4;
  EXPECT_EQ(4, trie.at({1, 2, 3, 4}));
}

TEST_F(metric_trie_test, check_node_existence) {
  EXPECT_FALSE(trie.has({1}));
  EXPECT_FALSE(trie.has({1, 2}));

  trie.down(1);

  EXPECT_TRUE(trie.has({1}));
  EXPECT_FALSE(trie.has({1, 2}));

  trie.down(2);
  EXPECT_TRUE(trie.has({1, 2}));

  trie.up();
  EXPECT_TRUE(trie.has({1}));
  EXPECT_TRUE(trie.has({1, 2}));

  trie.down(3);
  EXPECT_TRUE(trie.has({1}));
  EXPECT_TRUE(trie.has({1, 2}));
  EXPECT_TRUE(trie.has({1, 3}));
}

TEST_F(metric_trie_test, timer_integration_test) {
  measure::trie<int, measure::timer> trie;

  trie.down(1).start();
  trie.down(2).start();
  trie.up();
  trie.up();

  EXPECT_TRUE(trie.has({1}));
  EXPECT_TRUE(trie.has({1, 2}));
}

TEST_F(metric_trie_test, preserves_top_node) {
  trie.down(11) = 123;
  trie.up();
  EXPECT_EQ(123, trie.down(11));
}

TEST_F(metric_trie_test, initially_has_zero_depth) {
  EXPECT_EQ(0u, trie.depth());
}

TEST_F(metric_trie_test, grows_in_depth) {
  trie.down(1);
  EXPECT_EQ(1u, trie.depth());

  trie.down(2);
  EXPECT_EQ(2u, trie.depth());

  trie.down(3);
  EXPECT_EQ(3u, trie.depth());
}

TEST_F(metric_trie_test, reduces_depth) {
  trie.down(1);
  trie.down(2);
  trie.down(3);

  trie.up();
  EXPECT_EQ(2u, trie.depth());

  trie.up();
  EXPECT_EQ(1u, trie.depth());

  trie.up();
  EXPECT_EQ(0u, trie.depth());
}

TEST_F(metric_trie_test, reduces_depth_in_labmda_api) {
  auto up = [this] { this->trie.up([](auto) { return 1; }); };

  trie.down(1);
  trie.down(2);
  trie.down(3);

  up();
  EXPECT_EQ(2u, trie.depth());

  up();
  EXPECT_EQ(1u, trie.depth());

  up();
  EXPECT_EQ(0u, trie.depth());
}

TEST_F(metric_trie_test, have_no_common_root) {
  trie.down(1) = 1;
  trie.up();

  trie.down(2) = 2;
  trie.up();

  EXPECT_TRUE(trie.has({1}));
  EXPECT_EQ(1, trie.at({1}));

  EXPECT_TRUE(trie.has({2}));
  EXPECT_EQ(2, trie.at({2}));
}

TEST_F(metric_trie_test, clones_one_element_trie) {
  trie.down(11) = 123;
  auto clone = trie.clone();

  EXPECT_EQ(123, clone.at({11}));
}

TEST_F(metric_trie_test, clones_one_element_trie_with_no_cursor) {
  trie.down(11) = 123;
  trie.up();
  auto clone = trie.clone();

  EXPECT_EQ(123, clone.at({11}));
}

TEST_F(metric_trie_test, clones_deep_trie) {
  trie.down(11) = 123;
  trie.down(22) = 456;
  trie.down(33) = 789;
  auto clone = trie.clone();

  EXPECT_EQ(123, clone.at({11}));
  EXPECT_EQ(456, clone.at({11, 22}));
  EXPECT_EQ(789, clone.at({11, 22, 33}));
}

TEST_F(metric_trie_test, clones_wide_trie) {
  trie.down(11) = 123;
  trie.up();
  trie.down(22) = 456;
  trie.up();
  trie.down(33) = 789;
  trie.up();
  auto clone = trie.clone();

  EXPECT_EQ(123, clone.at({11}));
  EXPECT_EQ(456, clone.at({22}));
  EXPECT_EQ(789, clone.at({33}));
}

TEST_F(metric_trie_test, combines_tries) {
  rhs.down(1) = 11;
  lhs.down(2) = 22;
  auto combine = rhs.combine(lhs);
  EXPECT_EQ(11, combine.at({1}));
  EXPECT_EQ(22, combine.at({2}));
}

TEST_F(metric_trie_test, aggregates_tries) {
  rhs.down(1) = 11;
  lhs.down(1) = 22;
  auto combine = rhs.combine(lhs);
  EXPECT_EQ(33, combine.at({1}));
}
