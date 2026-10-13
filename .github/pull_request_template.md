## Description
<!-- What changed and why. Link the issue this addresses. -->

## Related Issue
<!-- e.g. Closes #123 -->

## How Has This Been Tested?
<!-- The commands you ran and what they reported. If this touches a conversion or a
     DSK read, say how you checked it against NAIF SPICE. -->

## Checklist

- [ ] `pytest tests/ -v` passes locally, with spiceypy installed so the SPICE comparison tests actually run.
- [ ] Both headers still compile standalone (`cc -std=c99 -Wall -Wextra -Iinclude`, `c++ -std=c++11 -Wall -Iinclude`).
- [ ] New behavior has a test; a bug fix has a test that failed before the change.
- [ ] [CHANGELOG.md](../CHANGELOG.md) updated under `Unreleased`, if this affects users.
- [ ] Documentation (README and the header doc blocks) updated, if public behavior changed.
- [ ] No dependency added to either header beyond the C/C++ standard library.
- [ ] If generative AI tools were used to author or modify this code, it is noted in the description above, per USGS disclosure requirements.

### Pre-release security check

USGS policy requires that no sensitive content enter a public repository, and
repository history is permanent.

- [ ] This change contains no PII, absolute filesystem paths, internal server hostnames or IP addresses, and no usernames, passwords, or other credentials.

## Licensing
This project is mostly composed of free and unencumbered software released into the public domain, and we are unlikely to accept contributions that are not also released into the public domain. Somewhere near the top of each file should have these words:

> This work is free and unencumbered software released into the public domain. In jurisdictions that recognize copyright laws, the author or authors of this software dedicate any and all copyright interest in the software to the public domain.

- [ ] I dedicate any and all copyright interest in this software to the public domain. I make this dedication for the benefit of the public at large and to the detriment of my heirs and successors. I intend this dedication to be an overt act of relinquishment in perpetuity of all present and future rights to this software under copyright law.
