// Compile-only check that both headers can be included together in C++ and
// that nothing at file scope has external linkage. Built twice by the `headers`
// CI job and linked as two translation units, which is what catches a missing
// include guard or a public function that lost its `inline` -- either one is a
// duplicate-symbol error at link time, not at compile time.
//
// Built with `c++ -std=c++11 -Wall -Iinclude`; see .github/workflows/ci.yml.

#include <kerneless/dsk_reader.h>
#include <kerneless/utcet.h>

#include <string>

// Defined in this translation unit, called from the other one, so both are
// genuinely linked together rather than compiled and discarded.
#ifdef KERNELESS_SMOKE_MAIN
double kernelessSmokeEt();

int main() {
    double et = kernelessSmokeEt();
    std::string utc = ephemTimeToCalendarTime(et, "ISOC", 3, 64);

    kdsk_handle h;
    if (kdsk_dasopr("/nonexistent.bds", &h) == 0) {
        kdsk_dascls(&h);
        return 1;
    }
    return utc.empty() ? 1 : 0;
}
#else
double kernelessSmokeEt() {
    return calendarTimeToEphemTime("2000-01-01T12:00:00");
}
#endif
