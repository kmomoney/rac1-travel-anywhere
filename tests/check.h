/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The decision tests' checks: each failure prints its line and both values, and main returns the
 * count of failures (CTest fails on anything but 0).
 */
#ifndef RAC1_TRAVEL_ANYWHERE_CHECK_H
#define RAC1_TRAVEL_ANYWHERE_CHECK_H

#include <math.h>
#include <stdio.h>

static int g_failures;

/* Two floats within `tol` of each other. */
#define CHECK_NEAR(got, want, tol)                                                                 \
    do {                                                                                           \
        const double got_ = (got);                                                                 \
        const double want_ = (want);                                                               \
        if (!(fabs(got_ - want_) <= (tol))) {                                                      \
            printf("%s:%d: %s = %.9g, want %.9g\n", __FILE__, __LINE__, #got, got_, want_);        \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

/* Two ints equal. */
#define CHECK_EQ(got, want)                                                                        \
    do {                                                                                           \
        const long got_ = (long)(got);                                                             \
        const long want_ = (long)(want);                                                           \
        if (got_ != want_) {                                                                       \
            printf("%s:%d: %s = %ld, want %ld\n", __FILE__, __LINE__, #got, got_, want_);          \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

/* The tolerance for values worked out by hand to 7 digits. */
#define TOL 1e-6

/* The end of main: a line saying how it went, the count as the exit code. */
#define CHECK_DONE(name)                                                                           \
    do {                                                                                           \
        printf("%s: %s\n", (name), g_failures ? "FAILED" : "ok");                                  \
        return g_failures;                                                                         \
    } while (0)

#endif /* RAC1_TRAVEL_ANYWHERE_CHECK_H */
