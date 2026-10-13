/* Compile-only check that dsk_reader.h is valid, warning-free C that needs
 * nothing but the C standard library. Built by the `headers` CI job with
 * `cc -std=c99 -Wall -Wextra -Iinclude`; see .github/workflows/ci.yml.
 *
 * This is what catches dsk_reader.h acquiring a C++-ism or a dependency. It is
 * deliberately not a pytest test: the point is that the header compiles with no
 * build system at all. */

#include <kerneless/dsk_reader.h>

int main(void) {
    kdsk_handle h;
    kdsk_dladescr seg;
    kdsk_dskdescr descr;
    int32_t nv = 0, np = 0, n = 0;
    int found = 0;
    double vertices[3];
    int32_t plates[3];

    /* Reference every public entry point so an unused-function warning or a
     * signature change surfaces here. The open is expected to fail. */
    if (kdsk_dasopr("/nonexistent.bds", &h) == 0) {
        kdsk_dlabfs(&h, &seg, &found);
        if (found) {
            kdsk_dlafns(&h, &seg, &seg, &found);
            kdsk_dskgd(&h, &seg, &descr);
            kdsk_dskz02(&h, &seg, &nv, &np);
            kdsk_dskv02(&h, &seg, 1, 1, &n, vertices);
            kdsk_dskp02(&h, &seg, 1, 1, &n, plates);
        }
        kdsk_dascls(&h);
        return 1;
    }
    return 0;
}
