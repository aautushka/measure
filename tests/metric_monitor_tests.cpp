/*
MIT License

Copyright (c) 2018 Anton Autushka

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <gtest/gtest.h>
#include "metric.h"

struct metric_monitors_test : ::testing::Test
{
    using mon_t = metric::monitor<int>;
    mon_t mon;

    void busy_loop(int usec)
    {
        using metric::aggregate_timer;
        auto start = metric::timer::now();
        while (metric::timer::now() - start < usec);
    }

    std::string erase_all(std::string input, char ch)
    {
        std::string out;
        for (auto in : input)
        {
            if (in != ch)
            {
                out.append(1, in);
            }
        }
        return out;
    }
    
    std::string replace_all(std::string input, char ch, char sub)
    {
        std::string out;
        for (auto in : input)
        {
            out.append(1, in == ch ? sub : in);
        }
        return out;
    }

    std::string beautify_report(std::string rep)
    {
        auto out = erase_all(rep, '"');
        out = erase_all(out, ' ');
        out = erase_all(out, '\n');
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
    std::string report(Monitor&& mon)
    {
        return beautify_report(mon.report_json());
    }

    std::string report() 
    {
        return report(mon);
    }

};

TEST_F(metric_monitors_test, creates_manual_metric)
{
    mon.start(1);
    busy_loop(1);    
    mon.stop();

    auto rep = mon.report();
    EXPECT_LE(1, rep[1]);
}

TEST_F(metric_monitors_test, creates_nested_metrics)
{
    mon.start(1);
    mon.start(2);
    busy_loop(1);    
    mon.stop();
    mon.stop();

    auto rep = mon.report();
    EXPECT_LE(1, rep[1]);

    std::vector<int> key = {1, 2};
    EXPECT_LE(1, rep[key]);
}

TEST_F(metric_monitors_test, creates_scoped_metric)
{
    {
    auto metric = mon.scope(1);
    busy_loop(1);    
    }

    auto rep = mon.report();
    EXPECT_LE(1, rep[1]);
}

TEST_F(metric_monitors_test, produces_json_report)
{
    auto rep = mon.report_json();
    EXPECT_EQ("{}", report(mon));
}

TEST_F(metric_monitors_test, produces_non_empty_report)
{
    metric::monitor<char> mon;
    mon.start('a');
    mon.stop();

    EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, produces_sequential_report)
{
    metric::monitor<char> mon;
    mon.start('a');
    mon.start('b');
    mon.stop();
    mon.start('c');
    mon.stop();
    mon.stop();


    EXPECT_EQ("{a:{#:0,b:0,c:0}}", report(mon));
}

TEST_F(metric_monitors_test, produces_nested_report)
{
    metric::monitor<char> mon;
    mon.start('a');
    mon.start('b');
    mon.start('c');
    mon.stop();
    mon.stop();
    mon.stop();


    EXPECT_EQ("{a:{#:0,b:{#:0,c:0}}}", report(mon));
}

TEST_F(metric_monitors_test, produces_report_with_string_key)
{
    metric::monitor<const char*> mon;
    mon.start("a");
    mon.stop();

    EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, never_starts_sampling)
{
    mon.sample(0);
    mon.start(1);
    mon.start(2);
    mon.start(3);
    mon.stop();
    mon.stop();
    mon.stop();
    
    EXPECT_EQ("{}", report(mon));
}

TEST_F(metric_monitors_test, produces_json_with_no_common_root_element)
{
    metric::monitor<char> mon;
    mon.start('a');
    mon.stop();

    mon.start('b');
    mon.stop();


    EXPECT_EQ("{a:0,b:0}", report(mon));
}

TEST_F(metric_monitors_test, stops_sampling_after_reaching_limit_of_one)
{
    metric::monitor<char> mon;
    mon.sample(1);

    mon.start('a');
    mon.stop();

    mon.start('b');
    mon.stop();
    
    mon.start('c');
    mon.stop();

    EXPECT_EQ("{a:0}", report(mon));
}

TEST_F(metric_monitors_test, stops_sampling_after_reaching_limit_of_two)
{
    metric::monitor<char> mon;
    mon.sample(2);

    mon.start('a');
    mon.stop();

    mon.start('b');
    mon.stop();
    
    mon.start('c');
    mon.stop();

    EXPECT_EQ("{a:0,b:0}", report(mon));
}

TEST_F(metric_monitors_test, sampling_limit_has_no_affect_on_sampling_depth)
{
    metric::monitor<char> mon;
    mon.sample(1);

    mon.start('a');
    mon.start('b');
    mon.start('c');
    mon.stop();
    mon.stop();
    mon.stop();

    EXPECT_EQ("{a:{#:0,b:{#:0,c:0}}}", report(mon));
}
