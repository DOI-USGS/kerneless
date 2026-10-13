# Contributing to kerneless

Thank you for your interest in contributing. kerneless is developed by the
U.S. Geological Survey (USGS) Astrogeology Science Center, and we welcome
contributions from both inside and outside the USGS.

This document explains how to report problems, propose changes, and get a
contribution merged.

Because this repository lives in the DOI GitHub Enterprise Cloud, merge rights
are limited to DOI staff. That does not limit who can contribute — an external
contribution lands by way of a maintainer merging the pull request.

## Code of conduct

This project follows the [Planetary Software Code of Conduct](CODE_OF_CONDUCT.md).
Discussion should stay focused on the technical merits of the work. Harassment
or discriminatory behavior of any kind is not tolerated, and maintainers may
remove comments or contributions that violate this expectation.

## Reporting bugs

Open an issue at <https://github.com/DOI-USGS/kerneless/issues> and include:

- What you expected to happen, and what actually happened.
- The exact command or code needed to reproduce the problem.
- Your platform and versions: OS, compiler, Python, and — if the problem
  involves a comparison against NAIF SPICE — your spiceypy or cspice version.
- Any relevant error output or stack trace, as text rather than a screenshot.
- For a time conversion problem: the exact input string or ephemeris time, what
  kerneless returned, and what SPICE returned.
- For a DSK problem: a minimal `.bds` file, or a description of its segment
  structure (data type, NV, NP, coordinate system).

Search the existing issues first — the problem may already be filed or fixed
on `main`.

## Requesting features

Open an issue describing the problem you are trying to solve rather than only
the solution you have in mind. Explain the scientific or operational use case;
that context helps us judge scope and design a fix that serves other users too.

Note that kerneless deliberately does not reimplement all of SPICE. The DSK
intersection routines (`dskx02_c` / `dskn02_c`), for example, are out of scope
by design — see the [README](README.md).

## Reporting security issues

Do not open a public issue for a suspected security vulnerability. Email the
maintainer (see [Contact](#contact)) with the details and allow time for a fix
before disclosing publicly.

## Development setup

```bash
pip install -e ".[test]"
pytest tests/ -v
```

The `test` extra pulls in pytest and spiceypy; spiceypy is what lets the suite
compare kerneless against the real SPICE toolkit. Tests that need it skip
themselves when it is absent, so install it before claiming a change is clean.

Both headers must also compile standalone, with no build system. The compile-only
checks in [tests/smoke/](tests/smoke/) are what CI runs; run them the same way:

```bash
# dsk_reader.h as plain C, warning-free
cc -std=c99 -Wall -Wextra -Werror -Iinclude -o smoke_c tests/smoke/smoke.c && ./smoke_c

# both headers in C++, compiled twice and linked together, which is what catches
# a missing include guard or a public function that lost its `inline`
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude -DKERNELESS_SMOKE_MAIN \
  -c tests/smoke/smoke.cpp -o smoke_main.o
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude -c tests/smoke/smoke.cpp -o smoke_other.o
c++ smoke_main.o smoke_other.o -o smoke_cxx && ./smoke_cxx
```

If you add a public function to either header, reference it from the matching
smoke file so a signature change or a linkage mistake surfaces in CI.

## Submitting changes

1. **Open an issue first** for anything beyond a small fix. This avoids
   duplicated effort and lets us agree on the approach before you write code.
2. **Fork the repository** and create a branch off `main`. Use a short,
   descriptive branch name.
3. **Make your change**, including tests (see below).
4. **Run the test suite** and confirm it passes.
5. **Update [CHANGELOG.md](CHANGELOG.md)** under `Unreleased` if the change
   affects users of the library.
6. **Open a pull request** against `main`. Describe what changed and why, and
   link the issue it addresses.
7. **Respond to review.** A maintainer will review your pull request; changes to
   CI workflows additionally require review by a code owner.

Keep pull requests focused on a single concern. Unrelated changes bundled
together are slower to review and harder to revert.

### Commit messages

Write a short imperative subject line (for example, "Fix leap second lookup
before 1972"), and use the body to explain *why* the change is needed when that
is not obvious from the diff.

If you used generative AI tools to author or modify code, note that in the pull
request description so it can be recorded per USGS disclosure requirements. See
the Generative AI Disclosure section of the [README](README.md).

## Coding standards

- **Header-only, dependency-free.** Neither header may acquire a dependency
  beyond the C or C++ standard library. The whole point of this library is that
  it links no SPICE toolkit and loads no kernels.
- **C99 for `dsk_reader.h`, C++11 for `utcet.h`.** `dsk_reader.h` must stay
  compilable by a C compiler; keep it inside `extern "C"` and free of C++.
- **Everything at file scope must be `inline` or `static`.** Both headers are
  included from multiple translation units; a non-`inline` function definition
  is a duplicate-symbol bug waiting to happen.
- **Mirror the cspice names.** Public functions are deliberately named after the
  `*_c` routines they replace (`kdsk_dskv02` for `dskv02_c`), and SPICE's 1-based
  indexing is preserved. Document any deviation in the header's doc block.
- Match the style of the surrounding code: the naming, indentation, and comment
  conventions already used in the file you are editing.
- **No commented-out code** or backup files in commits.

## Tests

New behavior needs a test, and bug fixes should come with a test that fails
before the fix and passes after it.

- Tests live in [tests/](tests/) and run under pytest against the compiled
  `kerneless` extension module.
- Prefer a test that checks kerneless against NAIF SPICE over one that asserts a
  hard-coded expected value. SPICE is the ground truth this library exists to
  reproduce; see `tests/test_utcet_vs_naif.py` and the `TestDskVsNaif` class in
  `tests/test_dsk.py`.
- Gate any test that needs spiceypy behind the existing `HAS_SPICEYPY` skip so
  the suite still runs without it.
- Binary fixtures go in [tests/data/](tests/data/) and must be small. Record the
  provenance and license of anything vendored from elsewhere in
  [LICENSE.md](LICENSE.md).

## Documentation

Update the [README](README.md) when you change public behavior — it is the
whole of the user-facing documentation, and USGS policy requires it to be
sufficient for a third party to use and reproduce this software. Keep the
Python and C/C++ examples in sync with each other, and keep each header's own
doc block accurate about what it does and does not port from cspice.

## Licensing and attribution

kerneless is released into the public domain under [CC0 1.0](LICENSE.md). By
contributing, you agree that your contribution is released under the same terms
and that you have the right to make that dedication.

Work produced by USGS employees in the course of their duties is a work of the
U.S. Government and is not subject to copyright protection in the United States.

Material that originated elsewhere keeps its own attribution and is listed under
"Third-party content" in [LICENSE.md](LICENSE.md). If you vendor anything new,
add it there.

## Contact

Kelvin Rodriguez — <krodriguez@usgs.gov>
U.S. Geological Survey, Astrogeology Science Center
