#ifndef OWNERSHIP_BOUNDARY_STUBS_H
#define OWNERSHIP_BOUNDARY_STUBS_H

#include <string>

/* What the last stubbed extractor/library call received. */
struct StubCall {
    std::string parser;
    std::string bytes;
    std::string password;
    std::string charset;
    std::string model_path;
    int codepage = 0;
    int lib_inits = 0;
};

extern StubCall g_stub;

#endif
