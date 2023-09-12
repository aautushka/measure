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

struct metric_monitors_test : ::testing::Test {
  using mon_t = measure::monitor<int>;
  mon_t mon;
  mon_t lhs;
  mon_t rhs;

  void busy_loop(int usec) {
    using measure::aggregate_timer;
    auto start = measure::timer::now();
    while (measure::timer::now() - start < usec)
      ;
  }

  std::string erase_all(std::string input, char ch) {
    std::string out;
    for (auto in : input) {
      if (in != ch) {
        out.append(1, in);
      }
    }
    return out;
  }

  std::string replace_all(std::string input, char ch, char sub) {
    std::string out;
    for (auto in : input) {
      out.append(1, in == ch ? sub : in);
    }
    return out;
  }

  std::string beautify_minimally(std::string rep) {
    auto out = erase_all(rep, '"');
    out = erase_all(out, ' ');
    out = erase_all(out, '\n');
    return out;
  }

  std::string beautify_report(std::string rep) {
    auto out = beautify_minimally(rep);
    out = replace_all(out, '1', '0');
    out = replace_all(out, '2', '0');
    out = replace_all(out, '3', '0');
    out = replace_all(out, '4', '0');
    out = replace_all(out, '5', '0');
    out = replace_all(out, '6', '0');
    out = replace_all(out, '7', '0');
    out = replace_all(out, '8', '0');
    out = replace_all(out, '9', '0');
    return out;
  }

  template <typename Monitor>
  std::string
  report(Monitor &&mon,
         measure::report_type type = measure::report_type::averages) {
    return beautify_report(mon.report_json(type));
  }

  template <typename Monitor>
  std::string
  exact_report(Monitor &&mon,
               measure::report_type type = measure::report_type::averages) {
    return beautify_minimally(mon.report_json(type));
  }

  std::string report() { return report(mon); }
};

TEST_F(metric_monitors_test, creates_manual_metric) {
  mon.start(1);
  busy_loop(1);
  mon.stop();

  auto rep = mon.report();
  EXPECT_LE("1", rep[1]);
}

TEST_F(metric_monitors_test, creates_nested_metrics) {
  mon.start(1);
  mon.start(2);
  busy_loop(1);
  mon.stop();
  mon.stop();

  auto rep = mon.report();
  EXPECT_LE("1", rep[1]);

  std::vector<int> key = {1, 2};
  EXPECT_LE("1", rep[key]);
}

TEST_F(metric_monitors_test, creates_scoped_metric) {
  {
    auto metric = mon.scope(1);
    busy_loop(1);
  }

  auto rep = mon.report();
  EXPECT_LE("1", rep[1]);
}

TEST_F(metric_monitors_test, produces_json_report) {
  auto rep = mon.report_json();
  EXPECT_EQ("{}", report(mon));
}

TEST_F(metric_monitors_test, produces_non_empty_report) {
  measure::monitor<char> mon;
  mon.start('a');
  mon.stop();

  EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, produces_sequential_report) {
  measure::monitor<char> mon;
  mon.start('a');
  mon.start('b');
  mon.stop();
  mon.start('c');
  mon.stop();
  mon.stop();

  EXPECT_EQ("{a:{#:0,b:0,c:0}}", report(mon));
}

TEST_F(metric_monitors_test, produces_nested_report) {
  measure::monitor<char> mon;
  mon.start('a');
  mon.start('b');
  mon.start('c');
  mon.stop();
  mon.stop();
  mon.stop();

  EXPECT_EQ("{a:{#:0,b:{#:0,c:0}}}", report(mon));
}

TEST_F(metric_monitors_test, produces_report_with_string_key) {
  measure::monitor<const char *> mon;
  mon.start("a");
  mon.stop();

  EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, never_starts_sampling) {
  mon.stop_sampling_after(0);
  mon.start(1);
  mon.start(2);
  mon.start(3);
  mon.stop();
  mon.stop();
  mon.stop();

  EXPECT_EQ("{}", report(mon));
}

TEST_F(metric_monitors_test, produces_json_with_no_common_root_element) {
  measure::monitor<char> mon;
  mon.start('a');
  mon.stop();

  mon.start('b');
  mon.stop();

  EXPECT_EQ("{a:0,b:0}", report(mon));
}

TEST_F(metric_monitors_test, stops_sampling_after_reaching_limit_of_one) {
  measure::monitor<char> mon;
  mon.stop_sampling_after(1);

  mon.start('a');
  mon.stop();

  mon.start('b');
  mon.stop();

  mon.start('c');
  mon.stop();

  EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, stops_sampling_after_reaching_limit_of_two) {
  measure::monitor<char> mon;
  mon.stop_sampling_after(2);

  mon.start('a');
  mon.stop();

  mon.start('b');
  mon.stop();

  mon.start('c');
  mon.stop();

  EXPECT_EQ("{a:0,b:0}", report(mon));
}

TEST_F(metric_monitors_test, sampling_limit_has_no_affect_on_sampling_depth) {
  measure::monitor<char> mon;
  mon.stop_sampling_after(1);

  mon.start('a');
  mon.start('b');
  mon.start('c');
  mon.stop();
  mon.stop();
  mon.stop();

  EXPECT_EQ("{a:{#:0,b:{#:0,c:0}}}", report(mon));
}

TEST_F(metric_monitors_test, reports_percentages) {
  mon.start(1);
  busy_loop(1);
  mon.stop();

  EXPECT_EQ("{1:100%}", exact_report(mon, measure::report_type::percentages));
}

TEST_F(metric_monitors_test, reports_number_of_calls) {
  mon.start(1);
  mon.stop();
  mon.start(1);
  mon.stop();
  mon.start(1);
  mon.stop();

  EXPECT_EQ("{1:3}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, reports_number_of_calls_in_nested_object) {
  mon.start(1);
  mon.start(2);
  mon.stop();
  mon.stop();

  EXPECT_EQ("{1:{#:1,2:1}}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, reports_number_of_calls_in_flat_object) {
  mon.start(1);
  mon.stop();
  mon.start(2);
  mon.stop();

  EXPECT_EQ("{1:1,2:1}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, starts_sampling_with_delay) {
  mon.start_sampling_after(1);
  mon.start(1);
  mon.stop();
  mon.start(2);
  mon.stop();

  EXPECT_EQ("{2:1}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, cant_start_sampling_because_of_delay) {
  mon.start_sampling_after(0xff);
  mon.start(1);
  mon.stop();
  mon.start(2);
  mon.stop();

  EXPECT_EQ("{}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, sample_range) {
  mon.start_sampling_after(1);
  mon.stop_sampling_after(1);
  mon.start(1);
  mon.stop();
  mon.start(2);
  mon.stop();
  mon.start(3);
  mon.stop();

  EXPECT_EQ("{2:1}", exact_report(mon, measure::report_type::calls));
}

TEST_F(metric_monitors_test, clones_monitor) {
  mon.start(1);
  mon.stop();

  auto clone = mon.clone();

  EXPECT_EQ("{1:1}", exact_report(clone, measure::report_type::calls));
}

TEST_F(metric_monitors_test, combines_monitors) {
  lhs.start(1);
  lhs.stop();

  rhs.start(1);
  rhs.stop();

  auto combine = lhs.combine(rhs);

  EXPECT_EQ("{1:2}", exact_report(combine, measure::report_type::calls));
}

// need address sanitizer for this
// unfortunately, appleclang does not support this: ASAN_OPTIONS=detect_leaks=1
// ./tests
TEST_F(metric_monitors_test, memory_leak) {
  const auto depth = 100000;
  for (int i = 0; i < depth; ++i) {
    lhs.start(i);
  }
  for (int i = 0; i < depth; ++i) {
    lhs.stop();
  }
}

TEST_F(metric_monitors_test, continue_measuring_with_new_name) {
  measure::monitor<char> mon;
  mon.start('a');
  mon.proceed('b');
  mon.stop();

  EXPECT_EQ("{a:0,b:0}", report(mon));
}
