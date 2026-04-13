#ifndef __PDFIUM_PARSER_H__
#define __PDFIUM_PARSER_H__

#ifndef FILE_MACROS_H
    #define FILE_MACROS_H
    #define BUFLEN 4096
    #ifdef __APPLE__
        #define _fopen fopen
        #define _fseek fseek
        #define _ftell ftell
        #define _rb "rb"
        #define _wb "wb"
    #else
        #define _fopen _wfopen
        #define _fseek _fseeki64
        #define _ftell _ftelli64
        #define _rb L"rb"
        #define _wb L"wb"
    #endif
#endif

#include <sstream>
#include <iostream>

#include <string>
#include <chrono>

#include <fpdfview.h>
#include <fpdf_save.h>
#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <fpdf_annot.h>

#define WITH_NATIVE_UTF_DECODE 0

#if WITH_NATIVE_UTF_DECODE
    #if defined(_WIN32)
        #include <windows.h>
    #endif
    #ifdef __APPLE__
        #include <CoreFoundation/CoreFoundation.h>
    #endif
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef OUTPUT_TYPES
    #define OUTPUT_TYPES
    typedef enum {
        output_type_object = 0,
        output_type_text,
        output_type_collection,
        output_type_collections
    }output_type;
#endif

#include <json/json.h>
#include "4DPluginAPI.h"
#include "C_TEXT.h"
#include "4DPlugin-JSON.h"
#include "tokenizers_cpp.h"
#include "4DPlugin-Universal-Document-Parser.h"

extern bool pdfium_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                              output_type mode,
                              int max_paragraph_length,
                              bool unique_values_only,
                              bool text_as_tokens,
                              int tokens_length,
                              bool token_padding,
                              int pooling_mode,
                              float overlap_ratio,
                              std::string password);

#endif  /* __PDFIUM_PARSER_H__ */
