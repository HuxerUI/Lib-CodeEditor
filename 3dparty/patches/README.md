# Downstream patches for the vendored editor cores

These patches apply to the nested `3dparty/SweetEditor` and `3dparty/SweetLine`
checkouts. A fresh clone of this repository has no nested checkouts (the
gitlinks have no `.gitmodules` entries), and the top-level build then fetches
SweetEditor through FetchContent, applying `sweeteditor-oslog-gcc.patch`
automatically. Developers who materialize the nested checkouts by hand must
apply both patches themselves:

```sh
git -C 3dparty/SweetEditor apply ../patches/sweeteditor-oslog-gcc.patch
git -C 3dparty/SweetLine apply ../patches/sweetline-locale-include.patch
```

| Patch | Reason |
| --- | --- |
| `sweeteditor-oslog-gcc.patch` | `logging.hpp` routes Apple logging through `<os/log.h>`, which hard-errors under GCC; the patch restricts that path to clang so macOS builds with GCC work. |
| `sweetline-locale-include.patch` | `util.cpp` used `std::wstring_convert` and `std::copy` through transitive includes that libc++ 23 pruned; spell out `<locale>` and `<algorithm>`. Only needed when building the preview example with a nested SweetLine checkout. |
