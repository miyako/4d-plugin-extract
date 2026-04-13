//
//  tidy-parser
//
//  Created by miyako on 2025/09/09.
//

#include "tidy-parser.h"

namespace tidy {
    struct Document {
        std::string type;
        std::string text;
    };
}

using namespace tidy;

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
            if(!document.text.empty()) {
                ob_append_s(pages, document.text);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            if(!document.text.empty()) {
                texts.push_back(document.text);
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
            if(!document.text.empty()) {
                texts.push_back(document.text);
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
            if(!document.text.empty()) {
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                PA_ObjectRef paragraphNode = PA_CreateObject();
                ob_set_n(paragraphNode, "index", 0);
                ob_set_s(paragraphNode, "text", document.text.c_str());
                ob_append_o(paragraphs, paragraphNode);
                
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", 0);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

static void print_text(TidyDoc tdoc, TidyNode tnode, std::string& text) {
    
    for (TidyNode child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
        TidyNodeType ttype = tidyNodeGetType(child);
        if (ttype == TidyNode_Text) {
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetValue(tdoc, child, &buf);
            text += std::string((char*)buf.bp, buf.size);
            tidyBufFree(&buf);
        } else if (ttype == TidyNode_Start) {
            print_text(tdoc, child, text);
        }
    }
}

extern bool tidy_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                            output_type mode,
                            int max_paragraph_length,
                            bool unique_values_only,
                            bool text_as_tokens,
                            int tokens_length,
                            bool token_padding,
                            int pooling_mode,
                            float overlap_ratio) {
    
    bool success = false;
    
    Document document;
    
    TidyDoc tdoc = tidyCreate();
    TidyBuffer errbuf;
    tidyBufInit(&errbuf);
    
    tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
    tidyOptSetBool(tdoc, TidyXmlOut, no);
    tidyOptSetBool(tdoc, TidyForceOutput, yes);
    
    tidyOptSetBool(tdoc, TidyQuiet, yes);
    tidyOptSetBool(tdoc, TidyShowWarnings, no);
    tidySetErrorBuffer(tdoc, &errbuf);
    
    tidyOptSetValue(tdoc, TidyCustomTags, "blocklevel");
    tidyOptSetValue(tdoc, TidyDoctype, "auto");
    
    tidyOptSetBool(tdoc, TidyMark, no);
    tidyOptSetInt(tdoc, TidyWrapLen, 0);
    tidyOptSetBool(tdoc, TidyDropEmptyElems, yes);
    tidyOptSetBool(tdoc, TidyDropEmptyParas, yes);
    tidyOptSetBool(tdoc, TidyDropPropAttrs, yes);
    
    tidyOptSetBool(tdoc, TidyIndentContent, no);
    tidyOptSetInt(tdoc, TidyIndentSpaces, 0);
    
    tidyOptSetBool(tdoc, TidyQuoteAmpersand, no);
    tidyOptSetBool(tdoc, TidyAsciiChars, no);
    tidyOptSetBool(tdoc, TidyPreserveEntities, no);
    tidyOptSetBool(tdoc, TidyNumEntities, yes);
    
    if(tidyParseString(tdoc, (const char *)data.data()) >= 0) {
        document.type = "html";
        if(tidyCleanAndRepair(tdoc) >= 0) {
            TidyNode body = tidyGetBody(tdoc);
            print_text(tdoc, body, document.text);
        }
    } else {
        goto finally;
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
        
    success = true;

finally:
    
    tidyRelease(tdoc);
    tidyBufFree(&errbuf);
    
    if(!success) {
        ob_set_a(obj, L"type", L"unknown");
    }
    
    ob_set_b(obj, L"success", success);
    return success;
    
}
