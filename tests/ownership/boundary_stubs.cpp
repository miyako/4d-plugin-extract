/*
 * Synthetic stand-ins for every external extractor/library boundary the
 * plugin links against: the document parsers (OPC, PDFium, libxls, text,
 * tidy, OLECF, cmark-gfm, GMime), library init/shutdown, and the llama.cpp
 * tokenizer behind chunker::LlamaChunker. No real document, model or
 * library is touched; only synthetic inputs flow through.
 *
 * Each parser stub records its arguments and then builds its output with the
 * plugin's own helpers (ob_set_*, process_paragraphs -> ob_append_*), so the
 * real object/collection code still runs against the synthetic engine.
 */

#include "4DPlugin-Universal-Document-Parser.h"
#include "4DPlugin-JSON.h"
#include "boundary_stubs.h"

StubCall g_stub;

static bool stub_parse(const char *name, std::vector<uint8_t>& data, PA_ObjectRef obj,
                       bool text_as_tokens, int tokens_length, bool token_padding,
                       int pooling_mode, float overlap_ratio) {
    g_stub.parser = name;
    g_stub.bytes = std::string(data.begin(), data.end());
    std::vector<std::string> paragraphs;
    paragraphs.push_back(g_stub.bytes);
    PA_CollectionRef input = process_paragraphs(paragraphs, tokens_length, token_padding,
                                                !text_as_tokens, overlap_ratio, pooling_mode);
    ob_set_s(obj, L"type", name);
    ob_set_c(obj, L"input", input);
    ob_set_b(obj, L"success", true);
    return true;
}

#define STUB_COMMON std::vector<uint8_t>& data, PA_ObjectRef obj, output_type mode, \
    int max_paragraph_length, bool unique_values_only, bool text_as_tokens, int tokens_length, \
    bool token_padding, int pooling_mode, float overlap_ratio
#define STUB_ARGS data, obj, text_as_tokens, tokens_length, token_padding, pooling_mode, overlap_ratio

bool opc_parse_data(STUB_COMMON, std::string password) {
    g_stub.password = password;
    return stub_parse("opc", STUB_ARGS);
}
bool pdfium_parse_data(STUB_COMMON, std::string password) {
    g_stub.password = password;
    return stub_parse("pdfium", STUB_ARGS);
}
bool xls_parse_data(STUB_COMMON, std::string charset) {
    g_stub.charset = charset;
    return stub_parse("xls", STUB_ARGS);
}
bool txt_parse_data(STUB_COMMON) { return stub_parse("txt", STUB_ARGS); }
bool tidy_parse_data(STUB_COMMON) { return stub_parse("tidy", STUB_ARGS); }
bool olecf_parse_data(STUB_COMMON, int codepage) {
    g_stub.codepage = codepage;
    return stub_parse("olecf", STUB_ARGS);
}
bool md_parse_data(STUB_COMMON, bool break_by_section) { return stub_parse("md", STUB_ARGS); }
bool gmime_parse_data(STUB_COMMON) { return stub_parse("gmime", STUB_ARGS); }

extern "C" {
void FPDF_InitLibrary() { g_stub.lib_inits++; }
void FPDF_DestroyLibrary() { g_stub.lib_inits--; }
void g_mime_init(void) { g_stub.lib_inits++; }
void g_mime_shutdown(void) { g_stub.lib_inits--; }
void cmark_gfm_core_extensions_ensure_registered(void) {}
}

/* llama.cpp boundary: one token per byte, no model file is parsed. */
namespace chunker {
LlamaChunker::LlamaChunker(const ModelConfig& cfg) : cfg_(cfg) { pad_token_id_ = 0; }
LlamaChunker::~LlamaChunker() {}
void LlamaChunker::ModelDeleter::operator()(void *) const noexcept {}
std::vector<int32_t> LlamaChunker::tokenize(const std::string& text, bool) const {
    return std::vector<int32_t>(text.begin(), text.end());
}
std::string LlamaChunker::detokenize(const std::vector<int32_t>& tokens, bool) const {
    std::string s;
    for(int32_t t : tokens) if(t) s.push_back((char)t);
    return s;
}
ChunkerPtr make_chunker(const ModelConfig& cfg) {
    g_stub.model_path = cfg.model_path;
    return ChunkerPtr(new LlamaChunker(cfg));
}
} // namespace chunker
