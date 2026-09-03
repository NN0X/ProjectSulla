#ifndef SULLA_TEST_FRAMEWORK_H
#define SULLA_TEST_FRAMEWORK_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>

namespace tf
{
        inline bool useColor() { return isatty(fileno(stdout)); }
        inline const char* c(const char* code) { return useColor() ? code : ""; }
        inline const char* ansiRed()   { return c("\033[31m"); }
        inline const char* ansiGreen() { return c("\033[32m"); }
        inline const char* ansiYellow(){ return c("\033[33m"); }
        inline const char* ansiCyan()  { return c("\033[36m"); }
        inline const char* ansiBold()  { return c("\033[1m");  }
        inline const char* ansiDim()   { return c("\033[2m");  }
        inline const char* ansiRst()   { return c("\033[0m");  }

        struct Stats
        {
                int checks = 0;
                int failures = 0;
        };

        inline Stats& stats()
        {
                static Stats s;
                return s;
        }

        inline std::string bitsToStr(const std::vector<int>& v)
        {
                std::string s;
                s.reserve(v.size());
                for (int b : v) s += (b ? '1' : '0');
                return s.empty() ? std::string("<empty>") : s;
        }

        inline void section(const std::string& title)
        {
                std::printf("\n%s%s== %s ==%s\n", ansiBold(), ansiCyan(), title.c_str(), ansiRst());
        }

        inline bool check(bool cond, const std::string& label, const std::string& detail = "")
        {
                stats().checks++;
                if (cond)
                {
                        std::printf("  %s[ ok ]%s %s\n", ansiGreen(), ansiRst(), label.c_str());
                }
                else
                {
                        stats().failures++;
                        std::printf("  %s[FAIL]%s %s\n", ansiRed(), ansiRst(), label.c_str());
                        if (!detail.empty())
                                std::printf("         %s%s%s\n", ansiDim(), detail.c_str(), ansiRst());
                }
                return cond;
        }

        inline bool checkEq(const std::vector<int>& got,
                            const std::vector<int>& want,
                            const std::string& label)
        {
                bool eq = (got == want);
                std::string detail;
                if (!eq)
                        detail = "got [" + bitsToStr(got) + "] want [" + bitsToStr(want) + "]";
                return check(eq, label, detail);
        }

        inline int summary()
        {
                int passed = stats().checks - stats().failures;
                std::printf("\n%s%s---------------------------------------------%s\n", ansiBold(), ansiCyan(), ansiRst());
                if (stats().failures == 0)
                        std::printf("%s%sALL PASSED%s  %d/%d checks\n",
                                    ansiBold(), ansiGreen(), ansiRst(), passed, stats().checks);
                else
                        std::printf("%s%sFAILED%s  %d/%d checks passed, %s%d failed%s\n",
                                    ansiBold(), ansiRed(), ansiRst(), passed, stats().checks, ansiRed(), stats().failures, ansiRst());
                std::printf("%s%s---------------------------------------------%s\n\n", ansiBold(), ansiCyan(), ansiRst());
                return stats().failures == 0 ? 0 : 1;
        }
}

#endif
