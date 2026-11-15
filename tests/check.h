// Tiny test helper shared by the unit tests: CHECK(condition) counts and reports failures.
#ifndef CHECK_H
#define CHECK_H

#include <iostream>

inline int failures = 0;
inline int checks = 0;

#define CHECK(condition)                                                            \
    do                                                                              \
    {                                                                               \
        checks++;                                                                   \
        if (!(condition))                                                           \
        {                                                                           \
            failures++;                                                             \
            std::cout << "  FAILED line " << __LINE__ << ": " #condition << std::endl;        \
        }                                                                           \
    } while (0)

inline int finish()
{
    std::cout << checks - failures << "/" << checks << " checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}

#endif
