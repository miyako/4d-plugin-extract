# Ownership regressions

A synthetic-boundary harness that builds the real plugin sources
(`4DPlugin-Universal-Document-Parser.cpp`, `support/4DPlugin-JSON.cpp`) and
the bundled 4D Plugin API (`4D Plugin API/`, unchanged) under ASan/UBSan. It
drives them against a modelled 4D engine that tracks every string, picture,
object, collection and handle.

Extractor libraries (the parsers, PDFium, GMime, cmark-gfm and llama.cpp) are
stubbed at their boundary. Only synthetic inputs and temporary files are used.

```sh
tests/ownership/run.sh . fixed                    # every test must pass

# The same harness against the unfixed base:
base=$(mktemp -d); git archive 8a65719 | tar -x -C "$base"
tests/ownership/run.sh "$base" baseline           # named defects must fail, controls must pass
```

## Contracts checked

- The results of `PA_GetObjectProperty` and `PA_ExecuteCommandByID` are owned.
  They are cleared only when `PA_GetLastError()`, captured immediately, is
  `eER_NoErr`. On error the result is left alone.
- Object and collection references read from a property stay borrowed from
  the parent after their copy is cleared.
- `PA_CreateVariable(eVK_Undefined)` is used before a shallow
  `PA_SetStringVariable`. `eVK_Unistring` would leak an empty string.
- A variable that borrows the caller's object via `PA_SetObjectVariable` is
  never cleared.
- `file_object_to_data` is **dormant** (it has no caller). It is tested
  directly.

## Limits

- The harness runs on macOS only. Windows-only paths (for example `_wfopen`)
  are not exercised.
- The 4D engine is modelled, not real. No 4D or Windows validation is
  performed.
