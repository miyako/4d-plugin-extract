#ifndef __MIME_PARSER_H__
#define __MIME_PARSER_H__

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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <fstream>

#include <json/json.h>
#include <sstream>
#include <iostream>



#ifdef _WIN32
#define NOMINMAX
#define _SSIZE_T_DEFINED 1
#include <Windows.h>
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define ssize_t __int64
#endif

#include "gmime/gmime.h"
#include "gmime/gmime-parser.h"

#include <tidy.h>
#include <tidybuffio.h>

typedef struct
{
    int level;
    bool is_main_message;
    Document *document;
}mime_ctx;

extern bool gmime_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio);

#endif  /* __MIME_PARSER_H__ */
