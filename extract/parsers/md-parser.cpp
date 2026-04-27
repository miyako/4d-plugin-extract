//
//  md-parser
//
//  Created by miyako on 2026/04/13.
//

#include "md-parser.h"

namespace md {
    struct Section {
        std::string body;      // everything under that heading until the next one
    };

    struct Page {
        std::vector<Section> sections;
    };

    struct Document {
        std::string type;
        std::vector<Page> pages;
    };
}

using namespace md;

static void document_to_json(Document& document,
                             PA_ObjectRef documentNode,
                             output_type mode,
                             int max_paragraph_length,
                             bool unique_values_only,
                             bool text_as_tokens,
                             int tokens_length,
                             bool token_padding,
                             int pooling_mode,
                             float overlap_ratio,
                             bool break_by_section) {

    switch (mode) {
        case output_type_text:
        {
            PA_CollectionRef pages = PA_CreateCollection();
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::string paragraphs;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                                        
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if (break_by_section) {
                        paragraphs = section.body;
                        ob_append_s(pages, paragraphs);
                        emptyCol = true;
                    }else {
                        if(!paragraphs.empty()) paragraphs += "\n";
                        paragraphs += section.body;
                        if (max_paragraph_length > 0) {
                            paragraph_length++;
                            if (paragraph_length == max_paragraph_length) {
                                ob_append_s(pages, paragraphs);
                                paragraph_length = 0;
                                emptyCol = true;
                                paragraphs.clear();
                                continue;
                            }
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                ob_append_s(pages, paragraphs);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            for (const auto &page : document.pages) {
                bool emptyCol = true;
                std::string paragraphs;
                //no need for colIdx, rowIdx
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    if (break_by_section) {
                        paragraphs = section.body;
                        texts.push_back(paragraphs);
                        emptyCol = true;
                    }else {
                        if(!paragraphs.empty()) paragraphs += "\n";
                        paragraphs += section.body;
                        
                        if (max_paragraph_length > 0) {
                            paragraph_length++;
                            if (paragraph_length == max_paragraph_length) {
                                texts.push_back(paragraphs);
                                paragraph_length = 0;
                                emptyCol = true;
                                paragraphs.clear();
                                continue;
                            }
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
                texts.push_back(paragraphs);
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
                bool emptyCol = true;
                std::unordered_set<std::string> seen;
                int paragraph_length = 0;
                for (const auto &section : page.sections) {
                    if(section.body.empty())
                        continue;
                    
                    if(section.body.empty())
                        continue;
                    
                    if ((unique_values_only) && (!seen.insert(section.body).second)) {
                        continue;
                    }
                    
                    emptyCol = false;
                    
                    texts.push_back(section.body);
                    
                    if (break_by_section) {
                        PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                        ob_append_c(pages, matrix);
                        emptyCol = true;
                        texts.clear();
                    } else {
                        if (max_paragraph_length > 0) {
                            paragraph_length++;
                            if (paragraph_length == max_paragraph_length) {
                                PA_CollectionRef matrix = process_paragraphs(texts, tokens_length, token_padding, !text_as_tokens, overlap_ratio, pooling_mode);
                                ob_append_c(pages, matrix);
                                paragraph_length = 0;
                                emptyCol = true;
                                texts.clear();
                                continue;
                            }
                        }
                    }
                }
                if (emptyCol) {
                    //empty page
                    continue;
                }
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
                bool emptyCol = true;
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                int rowIdx = 0; // physical sheet row index, increments regardless of empty rows
                for (const auto &section : page.sections) {
                    PA_ObjectRef paragraphNode = PA_CreateObject();
                    if(section.body.empty())
                        continue;
                    
                    emptyCol = false;
                                        
                    ob_set_n(paragraphNode, "index", rowIdx++);
                    ob_set_s(paragraphNode, "text", section.body.c_str());
                    ob_append_o(paragraphs, paragraphNode);
                }
                if (emptyCol) {
                    //empty page
                    colIdx++;
                    continue;
                }
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", colIdx++);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
}

static std::string markdown_to_plaintext(const char *md) {

    std::string result;

    cmark_gfm_core_extensions_ensure_registered();

    cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
    cmark_syntax_extension *table_ext = cmark_find_syntax_extension("table");
    if (table_ext) cmark_parser_attach_syntax_extension(parser, table_ext);
    cmark_syntax_extension *tasklist_ext = cmark_find_syntax_extension("tasklist");
    if (tasklist_ext) cmark_parser_attach_syntax_extension(parser, tasklist_ext);

    cmark_parser_feed(parser, md, strlen(md));
    cmark_node *doc = cmark_parser_finish(parser);
    cmark_parser_free(parser);

    cmark_iter *iter = cmark_iter_new(doc);
    bool firstCellInRow = true;

    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node      *node = cmark_iter_get_node(iter);
        cmark_node_type   tp  = cmark_node_get_type(node);
        const char        *ts = cmark_node_get_type_string(node);

        if (ev == CMARK_EVENT_ENTER) {
            switch (tp) {
                case CMARK_NODE_TEXT:
                case CMARK_NODE_CODE:
                    result += cmark_node_get_literal(node);
                    break;

                case CMARK_NODE_CODE_BLOCK:
                    if (!result.empty()) result += (const char *)"\n";
                    result += cmark_node_get_literal(node);
                    break;

                case CMARK_NODE_SOFTBREAK:
                case CMARK_NODE_LINEBREAK:
                    result += (const char *)"\n";
                    break;

                case CMARK_NODE_PARAGRAPH:
                case CMARK_NODE_HEADING:
                    if (!result.empty()) result += (const char *)"\n";
                    break;

                default:
                    // GFM extension nodes — type is only known at runtime
                    if (strcmp(ts, "table") == 0) {
                        if (!result.empty()) result += (const char *)"\n";
                    } else if (strcmp(ts, "table_row") == 0) {
                        firstCellInRow = true;
                    } else if (strcmp(ts, "table_cell") == 0) {
                        if (!firstCellInRow) result += (const char *)"  |  ";
                        firstCellInRow = false;
                    } else if (strcmp(ts, "tasklist") == 0) {
                        int checked = cmark_gfm_extensions_get_tasklist_item_checked(node);
                        result += (const char *)(checked ? "[x] " : "[ ] ");
                    }
                    break;
            }

        } else if (ev == CMARK_EVENT_EXIT) {
            switch (tp) {
                case CMARK_NODE_HEADING:
                    result += (const char *)"\n";
                    break;
                default:
                    if (strcmp(ts, "table_row") == 0)
                        result += (const char *)"\n";
                    break;
            }
        }
    }

    cmark_iter_free(iter);
    cmark_node_free(doc);
    return result;
}

extern bool md_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
                          output_type mode,
                          int max_paragraph_length,
                          bool unique_values_only,
                          bool text_as_tokens,
                          int tokens_length,
                          bool token_padding,
                          int pooling_mode,
                          float overlap_ratio,
                          bool break_by_section) {
            
    Document document;
    document.type = "md";
    std::string markdown = std::string((const char *)data.data(), data.size());
    
    Section section;
    section.body = markdown_to_plaintext(markdown.c_str());
    
    Page page;
    page.sections.push_back(section);
    document.pages.push_back(page);
    
    document_to_json(document,
                     obj,
                     mode,
                     max_paragraph_length,
                     unique_values_only,
                     text_as_tokens,
                     tokens_length,
                     token_padding,
                     pooling_mode,
                     overlap_ratio,
                     break_by_section);
        
    ob_set_b(obj, L"success", true);
    return true;
}
