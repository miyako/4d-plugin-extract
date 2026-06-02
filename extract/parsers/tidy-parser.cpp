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

// Returns true if this element's tag is block-level and should
// introduce a newline before its text content.
static bool is_block_tag(TidyNode node) {
    TidyTagId id = tidyNodeGetId(node);
    switch (id) {
        case TidyTag_ADDRESS:
        case TidyTag_ARTICLE:
        case TidyTag_ASIDE:
        case TidyTag_BLOCKQUOTE:
        case TidyTag_BR:
        case TidyTag_CAPTION:
        case TidyTag_DD:
        case TidyTag_DIV:
        case TidyTag_DL:
        case TidyTag_DT:
        case TidyTag_FIELDSET:
        case TidyTag_FIGCAPTION:
        case TidyTag_FIGURE:
        case TidyTag_FOOTER:
        case TidyTag_FORM:
        case TidyTag_H1:
        case TidyTag_H2:
        case TidyTag_H3:
        case TidyTag_H4:
        case TidyTag_H5:
        case TidyTag_H6:
        case TidyTag_HEADER:
        case TidyTag_HR:
        case TidyTag_LI:
        case TidyTag_MAIN:
        case TidyTag_NAV:
        case TidyTag_OL:
        case TidyTag_P:
        case TidyTag_PRE:
        case TidyTag_SECTION:
        case TidyTag_SUMMARY:
        case TidyTag_TABLE:
        case TidyTag_TD:
        case TidyTag_TH:
        case TidyTag_TITLE:
        case TidyTag_TR:
        case TidyTag_UL:
            return true;
        default:
            return false;
    }
}

static void print_text(TidyDoc tdoc, TidyNode tnode, std::string& text) {

    for (TidyNode child = tidyGetChild(tnode); child; child = tidyGetNext(child)) {
        TidyNodeType ttype = tidyNodeGetType(child);

        if (ttype == TidyNode_Text) {
            TidyBuffer buf;
            tidyBufInit(&buf);
            tidyNodeGetValue(tdoc, child, &buf);
            if (buf.size > 0) {
                // Ensure block-level parent is separated from prior text.
                // Check parent (tnode) — if it is block-level and text is
                // non-empty and doesn't already end with whitespace, add \n.
                if (!text.empty() && is_block_tag(tnode)) {
                    char last = text.back();
                    if (last != '\n' && last != ' ') {
                        text += '\n';
                    }
                }
                text += std::string((char*)buf.bp, buf.size);
            }
            tidyBufFree(&buf);

        } else if (ttype == TidyNode_Start) {
            // <br> has no text children but should still inject a newline.
            if (tidyNodeGetId(child) == TidyTag_BR) {
                if (!text.empty() && text.back() != '\n')
                    text += '\n';
            }
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
    
    TidyBuffer input;
    tidyBufInit(&input);
    tidyBufAttach(&input, (byte*)data.data(), (uint)data.size());

    if(tidyParseBuffer(tdoc, &input) >= 0) {
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
