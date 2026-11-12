![version](https://img.shields.io/badge/version-21%2B-3B69E9)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-extract)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-extract/total)

# 4d-plugin-extract

Extracts text from `.xlsx`, `.docx`, `.pptx`, `.xls`, `.doc`, `.ppt`, `.pdf`, `.msg`, `.eml`, `.html`, `.txt`, and `.md` documents and splits it into token-aware chunks suitable for passing directly to an `embeddings` or `contextualizedembeddings` endpoint. Chunk boundaries are computed by an in-process `llama.cpp`-compatible tokenizer (loaded from a GGUF file via [`Extract SET OPTION`](#extract-set-option)), so chunk sizes are exact token counts rather than character/paragraph approximations.

| Command | Returns | Purpose |
|-|-|-|
| [`Extract`](#extract) | Object | Extract and chunk a document's text |
| [`Extract SET OPTION`](#extract-set-option) | — | Load the tokenizer used to size chunks |

**Platforms:** macOS 11.5+ and Windows 64-bit. Releases built by the current release workflow are universal (Apple Silicon and Intel); see [Requirements](#requirements--platform-notes) for the Intel caveat.

---

## Requirements & platform notes

- **Load a tokenizer before using `Extract Output Collection` or `Extract Output Collections`.** [`Extract SET OPTION`](#extract-set-option) with `Extract Option Tokenizer File` must run at least once (per plugin lifetime, not per call). Without a tokenizer these two formats return `success: true` with an **empty** `input` collection (or `inputs` holding empty collections), not an error — see [Error handling](#error-handling--troubleshooting). `Extract Output Object` and `Extract Output Text` don't use the tokenizer.
- **Preemptive processes.** Both commands are declared thread-safe, and the sample methods are `"preemptive":"capable"`. PDF extraction uses PDFium, which is not thread-safe, and the plugin does not serialize PDFium calls: avoid running `Extract` on `.pdf` files in several preemptive processes **at the same time** until this is addressed. The other formats keep their state per call.
- **Intel Macs.** The universal binary's Intel slice is linked without llama.cpp's CPU backend (the vendored `libggml-cpu.a` is Apple Silicon only). Tokenizer loading only reads the vocabulary, so this may not matter, but tokenizer-based chunking on Intel has not been tested.
- **`.rtf` has no working extractor.** `Extract Document RTF` is a declared constant, but the plugin's internal format dispatch has no case for it. Calling `Extract` with this constant always returns `{success: false}` with no further detail — there is currently no code path that produces RTF output at all.
- **Failure is silent, not a 4D error.** `Extract` never raises an exception or error to the calling method. Every failure — missing file, unreadable format, unhandled document type, an internal parser exception — surfaces only as `success: false` on the returned object, with no `error`/message property. Plan your calling code around checking `.success`, not around `try`/catch equivalents.
- **Not every task property applies to every document type.** `password`, `charset`, `codepage`, and `break_by_section` are each meaningful for only a subset of document types — see the table in [`Extract`](#extract)'s description. Passing one that doesn't apply to the current `documentType` is harmless; it's simply ignored.
- **Keep `overlap_ratio` away from the edges of its range.** It must be strictly between `0` and `1`; anything outside that (inclusive of `0` and `1` themselves) is silently ignored in favor of the `0.09` default. In practice, stay comfortably inside the range (e.g. `0.05`–`0.2`) — values pushed right up against `1` aren't a meaningful configuration and should be avoided.

---

## Extract

### Syntax

```4d
$result:=Extract(documentType; outputFormat; task)
```

| Parameter | Type | Description |
|-|-|-|
| `documentType` | Longint | One of the `Extract Document *` constants (see table below). Mandatory. |
| `outputFormat` | Longint | One of the `Extract Output *` constants (see table below). Mandatory. |
| `task` | Object | Describes the document to extract and how to chunk it — see properties below. Mandatory. |
| Result | Object | The extraction result. Always has `success` (Boolean); its other properties depend on `outputFormat` — see below. |

#### `documentType`

|File Extension|Constant|Value|
|-|-|-:|
|xlsx|`Extract Document XLSX`|`0`|
|docx|`Extract Document DOCX`|`1`|
|pptx|`Extract Document PPTX`|`2`|
|xls |`Extract Document XLS`|`3`|
|doc |`Extract Document DOC`|`4`|
|ppt |`Extract Document PPT`|`5`|
|pdf |`Extract Document PDF`|`6`|
|msg |`Extract Document MSG`|`7`|
|eml |`Extract Document EML`|`8`|
|rtf |`Extract Document RTF`|`9`|
|html|`Extract Document HTML`|`10`|
|txt |`Extract Document TXT`|`11`|
|md  |`Extract Document MD`|`12`|

`.md` is not converted to plain text before chunking — Markdown syntax stays in the extracted text. `Extract Document RTF` is declared but not wired up to any extractor — see [Requirements](#requirements--platform-notes).

#### `outputFormat`

|Constant|Value|Description|
|-|-:|-|
|`Extract Output Object`|`0`|For custom processing, focus on structure.|
|`Extract Output Text`|`1`|For custom processing, focus on text.|
|`Extract Output Collection`|`2`|Suitable for an **OpenAI**-style `embeddings` API.|
|`Extract Output Collections`|`3`|Suitable for a **Voyage AI**-style `contextualizedembeddings` API.|

Every result has `success` (Boolean). On success the other properties are:

|`outputFormat`|Properties|
|-|-|
|`Extract Output Object`|`type` (Text, e.g. `"docx"`), `pages` (Collection of objects: `index`, `paragraphs` — a collection of `{index; text}`; spreadsheet rows also carry `values`, a collection of cell texts, and sheets a `meta.name`). `.msg`/`.eml` results also have a `meta` object (subject, sender, recipients).|
|`Extract Output Text`|`documents` (Collection of Text: one element per page/slide/sheet, or per group of `max_paragraph_length` paragraphs). No tokenizer involved.|
|`Extract Output Collection`|`type`, `input` (Collection: every chunk of the document, in order — Text, or a collection of token IDs when `text_as_tokens` is `true`).|
|`Extract Output Collections`|`type`, `inputs` (Collection of Collections: the chunks of each page/slide/sheet grouped together).|

`unique_values_only` and `max_paragraph_length` shape the Text, Collection and Collections formats; the Object format ignores them for most document types.

#### `task` object properties

|Property|Type|Description|
|-|-|-|
|`file`|4D.File|The document to read. If omitted, `Extract` returns `{success: false}` immediately with no other properties set.|
|`password`|Text|Password to open `.docx`, `.xlsx`, `.pptx`, or `.pdf`. Ignored for other document types.|
|`charset`|Text|Charset used to open `.xls`. Default `"iso-8859-1"`. Ignored for other document types.|
|`codepage`|Longint|Codepage used to open `.doc`, `.ppt`, or `.msg`. Default `1252`. Ignored for other document types.|
|`break_by_section`|Boolean|Use Markdown headers to break sections, `.md` only. Default `true`. Ignored for other document types.|
|`unique_values_only`|Boolean|Skip duplicate values at row/paragraph/column level. Default `false`.|
|`max_paragraph_length`|Longint|Limit paragraphs sampled per page/slide/sheet. Default `-1` (no limit).|
|`text_as_tokens`|Boolean|Return chunks as a collection of raw token IDs instead of detokenized text — mainly useful for debugging a tokenizer. Default `false`.|
|`tokens_length`|Longint|Maximum tokens per chunk. Default `1024`. When `text_as_tokens` is `false`, leave headroom for the tokenizer's own BOS/EOS tokens rather than using the true model context limit (see the sample code below, which uses `1022` for a `1024`-token model).|
|`token_padding`|Boolean|Pad the last chunk of a document out to a fixed token count. Default `false`.|
|`pooling_mode`|Longint|One of the `Extract Pooling Mode *` constants (below); controls whether padding is prepended or appended when `token_padding` is `true`. Default `Extract Pooling Mode Mean`.|
|`overlap_ratio`|Real|Fraction of `tokens_length` that consecutive chunks overlap by. Must be strictly between `0` and `1`; out-of-range values fall back to the default silently. Default `0.09`.|

#### `Extract Pooling Mode`

|Constant|Value|Prepend or append padding|
|-|-:|-|
|`Extract Pooling Mode Mean`|`0`|N/A — mean pooling doesn't need padding position to matter; pads are masked out.|
|`Extract Pooling Mode CLS`|`1`|Append (real tokens first) — position 0 must stay the real first token.|
|`Extract Pooling Mode Last`|`2`|Prepend (real tokens last) — the last real token must land at the final position.|

Use `Extract Pooling Mode Last` with `token_padding: true` for decoder-only models; use `token_padding: false` for encoder-only models (`Mean`/`CLS`).

### Description

Reads `task.file`, dispatches on `documentType` to the matching internal extractor, and splits the resulting text into chunks sized by the tokenizer loaded via [`Extract SET OPTION`](#extract-set-option). If no tokenizer has been loaded yet, chunk boundaries can't be computed from real token counts — see [Error handling](#error-handling--troubleshooting) for what you get instead.

`documentType` and `outputFormat` are both mandatory Longint parameters — there's no optional/overloaded form of `Extract`.

### Example

From the plugin's own sample method (`import_docx_encoder_text.4dm`), tags preserved exactly as shipped:

```4d
var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"

var $file : 4D:C1709.File
var $extracted : Object

$files:=Folder:C1567("/RESOURCES/docx").files(fk ignore invisible:K87:22 | fk recursive:K87:7)\
.query("extension == :1"; ".docx")

For each ($file; $files)
	
	//when text_as_tokens=false, make room for BOS/EOS in tokens_length 
	$task:={file: $file; \
		text_as_tokens: False:C215; \
		tokens_length: 1022; \
		overlap_ratio: 0.09; \
		unique_values_only: True:C214; \
		pooling_mode: Extract Pooling Mode Mean}
	$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)
	
	If ($extracted.success)
		$input:=$extracted.input
		var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
		$batch:=$AIClient.embeddings.create($input)
		// ... handle $batch as usual (see import_docx_encoder_text.4dm for the full embeddings/save flow)
	Else 
		TRACE:C157
	End if 
End for each 
```

Note that `Extract`, `Extract SET OPTION`, and all `Extract Document *`/`Extract Output *`/`Extract Pooling Mode *`/`Extract Option *` constants carry no `:Cxxxx`/`:Kxx:xx` tag in any sample file — that's consistent with plugin-supplied commands and constants, which don't get the compiler-assigned tags that 4D's own built-ins (`Folder:C1567`, `True:C214`, `False:C215`, `TRACE:C157`, `4D:C1709`) do.

A `.xls` sheet, specifying `charset` (from `import_xls_encoder_text.4dm`):

```4d
$task:={file: $file; \
	text_as_tokens: False:C215; \
	tokens_length: 1022; \
	overlap_ratio: 0.09; \
	charset: "iso8859-1"; \
	unique_values_only: True:C214; \
	pooling_mode: Extract Pooling Mode Mean}
$extracted:=Extract(Extract Document XLS; Extract Output Collection; $task)
```

A `.md` file, disabling overlap and raising `tokens_length` (from `import_md_encoder_text.4dm`):

```4d
$task:={file: $file; \
	text_as_tokens: False:C215; \
	tokens_length: 1522; \
	overlap_ratio: 0; \
	unique_values_only: True:C214; \
	pooling_mode: Extract Pooling Mode Mean}
$extracted:=Extract(Extract Document MD; Extract Output Collection; $task)
```

---

## Extract SET OPTION

### Syntax

```4d
Extract SET OPTION(optionType; value)
```

| Parameter | Type | Description |
|-|-|-|
| `optionType` | Longint | `Extract Option Tokenizer File` is currently the only defined value (`0`). |
| `value` | 4D.File | Path to a GGUF tokenizer file. |

This command has no return value — there's no `Object`/success flag to check afterward.

### Description

Loads a GGUF file (tensors are not loaded, only the tokenizer/vocabulary) and builds an in-process, `llama.cpp`-compatible tokenizer. This is what lets [`Extract`](#extract) report chunk sizes as exact token counts matching the target embedding model, rather than an approximation.

The loaded tokenizer is shared across every subsequent `Extract` call, in every process, for the plugin's lifetime — call this once (e.g. at startup or before a batch job), not per document. Calling it again replaces the tokenizer; calls already running keep the one they started with. If the file can't be loaded (bad path, malformed GGUF), the failure is written to the standard error stream only; the currently-loaded tokenizer (if any) is left unchanged, and there's no 4D-side signal that the load failed.

### Example

From the plugin's own test method (`OnModelDownloaded.4dm`), called once the model file is available:

```4d
	Extract SET OPTION(Extract Option Tokenizer File; $file)
```

Loading a GGUF file you already have:

```4d
var $file : 4D.File
$file:=Folder(fk resources folder).file("models/tokenizer.gguf")
Extract SET OPTION(Extract Option Tokenizer File; $file)
```

---

## Error handling & troubleshooting

- **`success: false` with nothing else set.** This is the only failure signal `Extract` gives you — there's no error code or message property. If a batch job needs to distinguish *why* a document failed, you'll need to reason about it externally (check the file exists, check the extension against the table above, etc.) rather than relying on the plugin to tell you.
- **`Extract Document RTF` always fails.** The constant exists, but there's no extractor wired up to it internally — every call with this `documentType` returns `{success: false}` regardless of the file's actual contents.
- **Omitting `task.file` fails immediately.** No exception, no distinct message — just `{success: false}`, same as any other failure.
- **Empty `input` with `success: true`.** No tokenizer is loaded: call [`Extract SET OPTION`](#extract-set-option) first. The Collection and Collections formats can't chunk without it and return empty collections rather than an error.
- **`.msg` in `Extract Output Object` has empty `pages`.** The message body is only used by the Text, Collection and Collections formats; the Object format fills `meta` but not `pages`. Use another format for the body text.
- **`.eml` in `Extract Output Object` returns `success: false`.** Building the sender/recipient `meta` fails for messages that have a `From:` (or other address) header, so this combination does not work at present. Use `Extract Output Collection`, `Extract Output Collections` or `Extract Output Text`.
- **`.docx`/`.xlsx`/`.pptx` containing a DTD fail** (since the code review following v1.0.1). OOXML parts must not contain a `<!DOCTYPE`, and such a part is now rejected to block entity-expansion attacks; a part that can't be parsed contributes no text.
- **Untrusted documents.** Since the same review, deeply nested HTML (also inside `.msg`/`.eml`), malformed `.doc`/`.ppt`/`.msg` structures, `.xls` files with very many sheets and nested `.eml` multiparts no longer crash or hang 4D. Very large inputs (zip bombs, huge spreadsheets) are still processed without a size cap.
- **`password`/`charset`/`codepage`/`break_by_section` being ignored isn't a bug.** Each only applies to specific document types (see the `task` property table under [`Extract`](#extract)) — passing them for a type they don't apply to has no effect, good or bad.
- **`overlap_ratio` values right at the edges of `(0, 1)` fall back to the default `0.09`** rather than erroring — if you need to confirm your value took effect, check the actual chunk boundaries in the result rather than assuming the property was honored.
- **Memory growth over long batches.** Before the ownership fixes, every `Extract` call that passed `task.file` or options such as `password`, `charset` or `codepage`, and every `Extract SET OPTION` with `tokenizer_file`, leaked 4D values (the copied property values and the internal `4D.File` object). These are now released. The synthetic regression tests are in [`tests/ownership`](tests/ownership/README.md).

---

## Quick reference

```4d
// once, before any Extract calls that need real token-sized chunks
Extract SET OPTION(Extract Option Tokenizer File; $tokenizerFile)

// per document
$task:={file: $file; \
	tokens_length: 1022; \
	overlap_ratio: 0.09; \
	unique_values_only: True:C214; \
	token_padding: False:C215; \
	pooling_mode: Extract Pooling Mode Mean}

$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)

If ($extracted.success)
	$chunks:=$extracted.input  // Collection, ready for an embeddings API
Else 
	// no error detail available - check file/type/tokenizer setup
End if 
```
