//
//  gmime-parser
//
//  Created by miyako on 2025/10/02.
//

#include "mime-parser.h"

namespace gmime {
    struct Document {
        std::string type;
        Json::Value headers;
        std::string subject;
        std::string body;
    };
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

static void html_to_txt_tidy(std::string& html, std::string& txt) {
    
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
    
    if(tidyParseString(tdoc, html.c_str()) >= 0) {
        if(tidyCleanAndRepair(tdoc) >= 0) {
            TidyNode body = tidyGetBody(tdoc);
            print_text(tdoc, body, txt);
        }
    }
    
    tidyRelease(tdoc);
    tidyBufFree(&errbuf);
}

static void processMessage(GMimeObject *parent, GMimeObject *part, gpointer user_data);

static void processPart(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
    
    mime_ctx *ctx = (mime_ctx *)user_data;
        
    if(GMIME_IS_MESSAGE_PART(part))
    {
        GMimeMessage *message = g_mime_message_part_get_message ((GMimeMessagePart *)part);
                 
//        processAttachmentMessage(parent, part, ctx);
                
        if (message) {
            bool is_main_message = ctx->is_main_message;
            ctx->is_main_message = false;
            ctx->level++;
            g_mime_message_foreach(message, processMessage, ctx);
            ctx->level--;
            ctx->is_main_message = is_main_message;
        }
    }
}

static bool isPartialTextPart(GMimeObject *part) {
    
    if(0 == strncasecmp(part->content_type->type, "message", 7)) {
        if(0 == strncasecmp(part->content_type->subtype, "partial", 7)) {
            return true;
        }
    }
    return false;
}

static bool getHeaders(GMimeObject *part, const char *label, Json::Value& json_message) {
    
    bool hasHeaders = false;
    
    GMimeHeaderList *headers = g_mime_object_get_header_list (part);
    
    if(headers)
    {
        int len = g_mime_header_list_get_count(headers);
        
        if(len)
        {
            hasHeaders = true;
            
            Json::Value header_array = Json::Value(Json::arrayValue);
                        
            for(int i = 0; i < len; ++i)
            {
                GMimeHeader *h = g_mime_header_list_get_header_at(headers, i);
                
                Json::Value item = Json::Value(Json::objectValue);
                            
                item["name"]  = g_mime_header_get_name(h);
                item["value"] = g_mime_utils_header_decode_text(NULL, g_mime_header_get_value(h));
                
                header_array.append(item);
            }
            json_message[label] = header_array;
        }
        g_mime_header_list_clear(headers);
    }
    return hasHeaders;
}

static void processBodyOrAttachment(GMimeObject *parent, GMimeObject *part, mime_ctx *ctx) {
    
    //wrapper exists for part except message/rfc822
    GMimeDataWrapper *wrapper = g_mime_part_get_content((GMimePart *)part);
    
    if(wrapper)
    {
        GMimeContentType *partMediaType = g_mime_object_get_content_type(part);
        const char *mediaType = g_mime_content_type_get_media_type(partMediaType);
        const char *mediaSubType = g_mime_content_type_get_media_subtype (partMediaType);
        
        if(0 == strncasecmp(mediaType, "text", 4))
        {
            //g_mime_text_part_get_text will alloc
            // https://developer.gnome.org/gmime/stable/GMimeTextPart.html
            
            //special consideration for microsoft mht
            const char *charset = g_mime_text_part_get_charset((GMimeTextPart *)part);
            if(charset && (0 == strncasecmp(charset, "unicode", 7)))
            {
                g_mime_text_part_set_charset ((GMimeTextPart *)part, "utf-16le");
            }
            if(charset) {
                char *text = g_mime_text_part_get_text((GMimeTextPart *)part);
                if(text) {
                    if(0 == strncasecmp(mediaSubType, "html", 4))
                    {
                        std::string html = text;
                        std::string plain;
                        html_to_txt_tidy  (html, plain);
                        ctx->document->body += plain;
                    }else{
                        ctx->document->body += text;
                    }
                    g_free(text);
                }
            }
        }
    }
}

static void addToBody(GMimeObject *parent, GMimeObject *part, mime_ctx *ctx) {
    
    processBodyOrAttachment(parent, part, ctx);
}

static void processMessage(GMimeObject *parent, GMimeObject *part, gpointer user_data) {
    
    mime_ctx *ctx = (mime_ctx *)user_data;
    
    if(GMIME_IS_MULTIPART(part)) {
        ctx->level++;
        g_mime_multipart_foreach((GMimeMultipart *)part, processPart, ctx);
        ctx->level--;
    }
    
    bool addedToBody = false;
    
    GMimeContentDisposition *disposition = g_mime_object_get_content_disposition (part);
    
    if    ((GMIME_IS_TEXT_PART(part) && (!disposition))
       || (isPartialTextPart(part))
       || ((GMIME_IS_TEXT_PART(part) && ( disposition) && (0 == strncasecmp(g_mime_content_disposition_get_disposition(disposition), "inline", 6)))))
    {
        if(ctx->is_main_message) {
            addToBody(parent, part, ctx);
            addedToBody = true;
        }
    }
    
    if((!addedToBody) && GMIME_IS_PART(part))
    {
        if(ctx->is_main_message) {
//            addToAttachment(parent, part, ctx);
        }else{
//            addToPart(parent, part, ctx);
        }
    }
    
}

extern bool document_to_json(document,
                             obj,
                             mode,
                             max_paragraph_length,
                             unique_values_only,
                             text_as_tokens,
                             tokens_length,
                             token_padding,
                             pooling_mode,
                             overlap_ratio) {
    
    switch (mode) {
        case output_type_text:
        {
            std::string text;
            text = document.subject;
            if ((!text.empty()) && (!document.body.empty())) {
                text += "\n";
            }
            text += document.body;
            PA_CollectionRef pages = PA_CreateCollection();
            if(!text.empty()) {
                ob_append_s(pages, text);
            }
            ob_set_c(documentNode, L"documents", pages);
        }
            break;
        case output_type_collection:
        {
            std::string text;
            text = document.subject;
            if ((!text.empty()) && (!document.body.empty())) {
                text += "\n";
            }
            text += document.body;
            
            ob_set_s(documentNode, "type", document.type.c_str());
            std::vector<std::string> texts;
            if(!text.empty()) {
                texts.push_back(text);
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
            std::string text;
            text = document.subject;
            if ((!text.empty()) && (!document.body.empty())) {
                text += "\n";
            }
            text += document.body;
            
            if(!text.empty()) {
                texts.push_back(text);
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
            
            PA_ObjectRef metaNode = PA_CreateObject();
            ob_set_s(metaNode, "subject", document.message.subject.c_str());

            ob_set_s(metaNode, "from", document.headers["from"]);
            ob_set_s(metaNode, "sender", document.headers["sender"]);
            ob_set_s(metaNode, "bcc", document.headers["bcc"]);
            ob_set_s(metaNode, "cc", document.headers["cc"]);
            ob_set_s(metaNode, "to", document.headers["to"]);
            ob_set_s(metaNode, "replyTo", document.headers["replyTo"]);
            ob_set_o(documentNode, L"meta", metaNode);
            
            if(!document.body.empty()) {
                PA_ObjectRef pageNode = PA_CreateObject();
                PA_CollectionRef paragraphs = PA_CreateCollection();
                PA_ObjectRef paragraphNode = PA_CreateObject();
                ob_set_n(paragraphNode, "index", 0);
                ob_set_s(paragraphNode, "text", document.body.c_str());
                ob_append_o(paragraphs, paragraphNode);
                ob_set_c(pageNode, "paragraphs", paragraphs);
                ob_set_n(pageNode, "index", 0);
                ob_append_o(pages, pageNode);
            }
            ob_set_c(documentNode, L"pages", pages);
        }
            break;
    }
    
    if(rawText){

    }else{
        Json::Value documentNode(Json::objectValue);








        

        
    }
}

static void getAddress(InternetAddressList *list, const char *label, Json::Value& json_message) {
    
    InternetAddress *address;
    
    if(list)
    {
        int len = internet_address_list_length(list);
        
        if(len)
        {
            Json::Value address_array = Json::Value(Json::arrayValue);
            
            for(int i = 0; i < len; ++i)
            {
                address = internet_address_list_get_address(list, i);
                
                Json::Value item = Json::Value(Json::objectValue);
                
                //internet_address_to_string will alloc
                // https://developer.gnome.org/gmime/stable/InternetAddress.html#internet-address-to-string
                
                char *string = internet_address_to_string(address, NULL, FALSE);
                item["string"] = string ? string : "";
                if(string) g_free(string);
                
                char *encoded_string = internet_address_to_string(address, NULL, TRUE);
                item["encoded_string"] = encoded_string ? encoded_string : "";
                if(encoded_string) g_free(encoded_string);
                
                const char *addr = internet_address_mailbox_get_addr((InternetAddressMailbox *)address);
                item["addr"] = addr ? addr : "";

                const char *idn_addr = internet_address_mailbox_get_idn_addr((InternetAddressMailbox *)address);
                item["idn_addr"] = idn_addr ? idn_addr : "";
                
                const char *name = internet_address_get_name(address);
                item["name"] = name ? name : "";
                
                address_array.append(item);
                
            }
            json_message[label] = address_array;

        }else
        {
            //create an empty array
            json_message[label] = Json::Value(Json::arrayValue);
        }
        internet_address_list_clear(list);
    }else
    {
        //create an empty array
        json_message[label] = Json::Value(Json::arrayValue);
    }
    
}

using namespace gmime;

bool gmime_parse_data(std::vector<uint8_t>& data, PA_ObjectRef obj,
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
    document.type = "eml";
    
    GMimeStream *stream = g_mime_stream_mem_new_with_buffer((const char *)data.data(), data.size());
    if(stream) {
        
        GMimeParserOptions *options = g_mime_parser_options_new();
        g_mime_parser_options_set_address_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        g_mime_parser_options_set_allow_addresses_without_domain(options, true);
        g_mime_parser_options_set_parameter_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        g_mime_parser_options_set_rfc2047_compliance_mode(options, GMIME_RFC_COMPLIANCE_LOOSE);
        GMimeParser *parser = g_mime_parser_new_with_stream (stream);
        if(parser) {
            GMimeMessage *message = g_mime_parser_construct_message (parser, options);
            if(message)
            {
                Json::Value json = Json::Value(Json::objectValue);
                
                getAddress(g_mime_message_get_from (message), "from", document.headers);
                getAddress(g_mime_message_get_sender (message), "sender", document.headers);
                getAddress(g_mime_message_get_reply_to (message), "replyTo", document.headers);
                getAddress(g_mime_message_get_to (message), "to", document.headers);
                getAddress(g_mime_message_get_cc (message), "cc", document.headers);
                getAddress(g_mime_message_get_bcc (message), "bcc", document.headers);
                
                //getHeaders(g_mime_message_get_mime_part(message), "headers", document.headers);
                
                mime_ctx ctx;
                ctx.level = 1;
                ctx.is_main_message = true;
                ctx.document = &document;
                
                const char *message_subject = g_mime_message_get_subject(message);
                if(message_subject)
                    document.subject = message_subject;
                
                g_mime_message_foreach(message, processMessage, &ctx);
                
                g_clear_object(&message);
            }
            g_clear_object(&parser);
        }
        g_mime_parser_options_free(options);

        g_clear_object(&stream);
        
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
    }
    
    ob_set_b(obj, L"success", success);
    
    if(!success) {
        ob_set_a(obj, L"type", L"unknown");
    }
    
    delete_rtf_window(hwnd);

    if(temp_input_path.length()) {
        _unlink(temp_input_path.c_str());
    }
    
    return success;

}
