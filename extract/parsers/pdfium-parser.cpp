//
//  pdfium-parser
//
//  Created by miyako on 2025/09/03 as CLI
//  Converted by miyako to function on 2026/04/13
//

#include "pdfium-parser.h"

#if WITH_NATIVE_UTF_DECODE
static void utf16_to_utf8(const uint8_t *u16data, size_t u16size, std::string& u8) {
    
#ifdef __APPLE__
    CFStringRef str = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)u16data, u16size);
    if(str){
        size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + sizeof(uint8_t);
        std::vector<uint8_t> buf(size+1);
        CFIndex len = 0;
        CFStringGetBytes(str, CFRangeMake(0, CFStringGetLength(str)), kCFStringEncodingUTF8, 0, true, (UInt8 *)buf.data(), buf.size(), &len);
        u8 = (const char *)buf.data();
        CFRelease(str);
    }else{
        u8 = "";
    }
#else
    int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16data, u16size, NULL, 0, NULL, NULL);
    if(len){
        std::vector<uint8_t> buf(len + 1);
        WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)u16data, u16size, (LPSTR)buf.data(), buf.size(), NULL, NULL);
        u8 = (const char *)buf.data();
    }else{
        u8 = "";
    }
#endif
}
#endif

namespace pdfium {
    struct Page {
        std::string text;
    };

    struct Document {
        std::string type;
        std::vector<Page> pages;
    };
}

using namespace pdfium;

static void document_to_json(Document& document,
                             PA_ObjectRef documentNode,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio) {

    switch (mode) {
        case output_type_text:
        {
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &page : document.pages) {
                std::unordered_set<std::string> seen;
                
                if(page.text.empty())
                    continue;
                                        
                if ((unique_values_only) && (!seen.insert(page.text).second)) {
                    continue;
                }
                                    
                ob_append_s(pages, page.text);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            for (const auto &page : document.pages) {
                std::unordered_set<std::string> seen;
                
                if(page.text.empty())
                    continue;
                
                if ((unique_values_only) && (!seen.insert(page.text).second)) {
                    continue;
                }

                texts.push_back(page.text);
            }
            PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
            ob_set_c(documentNode, "input", matrix);
        }
            break;
        case output_type_collections:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            std::vector<std::string> texts;
            for (const auto &page : document.pages) {
                std::unordered_set<std::string> seen;
                
                if(page.text.empty())
                    continue;
                
                if ((unique_values_only) && (!seen.insert(page.text).second)) {
                    continue;
                }
                
                texts.push_back(page.text);
                
                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                ob_append_c(pages, matrix);
                texts.clear();
            }
            ob_set_c(documentNode, L"inputs", pages);
        }
            break;
        case output_type_object:
        default:
        {
            //ignore max_paragraph_length, unique_values_only
            ob_set_s(documentNode, "type", document.type.c_str());
            PA_CollectionRef pages = PA_CreateCollection();
            int colIdx = 0;
            for (const auto &page : document.pages) {
                
                if(page.text.empty())
                    continue;
                
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                PA_ObjectRef paragraphNode = PA_CreateObject();
                ob_set_n(paragraphNode, "index", 0);
                ob_set_s(paragraphNode, "text", page.text.c_str());
                ob_append_o(paragraphs, paragraphNode);
                
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", colIdx++);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

#ifdef _WIN32
static std::string wchar_to_utf8(const wchar_t* wstr) {
    if (!wstr) return std::string();

    // Get required buffer size in bytes
    int size_needed = WideCharToMultiByte(
        CP_UTF8,            // convert to UTF-8
        0,                  // default flags
        wstr,               // source wide string
        -1,                 // null-terminated
        nullptr, 0,         // no output buffer yet
        nullptr, nullptr
    );

    if (size_needed <= 0) return std::string();

    // Allocate buffer
    std::string utf8str(size_needed, 0);

    // Perform conversion
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        &utf8str[0],
        size_needed,
        nullptr,
        nullptr
    );

    // Remove the extra null terminator added by WideCharToMultiByte
    if (!utf8str.empty() && utf8str.back() == '\0') {
        utf8str.pop_back();
    }

    return utf8str;
}
#endif

const std::string u_fffe = "\xEF\xBF\xBE";

extern bool pdfium_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                              output_type mode,
                              int max_paragraph_length,
                              bool unique_values_only,
                              bool text_as_tokens,
                              int tokens_length,
                              bool token_padding,
                              int pooling_mode,
                              float overlap_ratio,
                              std::string password) {
    
    Document document;
    FPDF_DOCUMENT doc = FPDF_LoadMemDocument(
                                             data.data(),
                                             (int)data.size(),
                                             password.length() ? password.c_str() : nullptr);
    
    if(doc){
        
        document.type = "pdf";

        int page_count = FPDF_GetPageCount(doc);
                
        auto extract_page_text = [&](int i, std::string& t) {
            
            FPDF_PAGE page = FPDF_LoadPage(doc, i);
            if (!page) return;
            
            FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);
            if (!text_page) {
                FPDF_ClosePage(page);
                return;
            }
            
            int nChars = FPDFText_CountChars(text_page);
            if (nChars <= 0) {
                FPDFText_ClosePage(text_page);
                FPDF_ClosePage(page);
                return;
            }
            
            // +1 for the null terminator PDFium always writes
            std::u16string utf16(nChars + 1, u'\0');
            utf16.resize(nChars);
            
            FPDFText_GetText(text_page, 0, nChars,
                             reinterpret_cast<unsigned short*>(&utf16[0]));
            utf16.resize(nChars); // drop the trailing null
            
            std::string utf8;
            
#if WITH_NATIVE_UTF_DECODE
            utf16_to_utf8((const uint8_t *)utf16.data(), utf16.size() * sizeof(char16_t), utf8);
#else
            for (char16_t ch : utf16) {
                if (ch == 0) break;
                if (ch < 0x80) {
                    utf8.push_back(static_cast<char>(ch));
                } else if (ch < 0x800) {
                    utf8.push_back(0xC0 | (ch >> 6));
                    utf8.push_back(0x80 | (ch & 0x3F));
                } else {
                    utf8.push_back(0xE0 | (ch >> 12));
                    utf8.push_back(0x80 | ((ch >> 6) & 0x3F));
                    utf8.push_back(0x80 | (ch & 0x3F));
                }
            }
#endif
            
            size_t pos = 0;
            while ((pos = utf8.find(u_fffe, pos)) != std::string::npos) {
                utf8.erase(pos, u_fffe.length());
            }
            
            t = utf8;
            
            FPDFText_ClosePage(text_page);
            FPDF_ClosePage(page);
        };
        
        for (int i = 0; i < page_count; ++i) {
            Page page;
            extract_page_text(i, page.text);
            document.pages.push_back(page);
        }
        
        document_to_json(document,
                         obj,
                         mode,
                         max_paragraph_length,
                         unique_values_only,
                         text_as_tokens,
                         tokens_length,
                         token_padding,
                         pooling_mode,
                         overlap_ratio);
        
        FPDF_CloseDocument(doc);
    }else{
        std::cerr << "Failed to load PDF!" << std::endl;
        goto unfortunately;
    }
        
    goto finally;
    
unfortunately:

    ob_set_a(obj, L"type", L"unknown");
    return false;
        
finally:
    
    ob_set_b(obj, L"success", true);
    return true;
    
}
