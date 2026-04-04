![version](https://img.shields.io/badge/version-21%2B-3B69E9)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-extract)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-extract/total)

# 4d-plugin-extract
Universal Document Parser

## Abstract

Extract text from various document types in a chunked format which be passed directly to one of the following endpoints:

- `embeddings`
- `contextualizedembeddings`

The goal is to optimise text processing in a RAG pipelines by eliminating nodes such as: 

- [`miyako/extract`](https://github.com/miyako/extract)
- [`miyako/text-splitter`](https://github.com/miyako/text-splitter)

## Extract

This is the main function. Pass the document type, output format, and a `task` object.

### Supported Document Types

|File Extension|Constant|Value
|-|-|-:|
|xlsx|`Extract Document XLSX`|`0`|
|docx|`Extract Document DOCX`|`1`|
|pptx|`Extract Document PPTX`|`2`|

### Supported Output Formats

|Constant|Value|Description
|:-|-:|-
|`Extract Output Object`|`0`|For custom processing 
|`Extract Output Text`|`1`|Suitable for **OpenAI** style `embeddings` API, decoder-only model 
|`Extract Output Collection`|`2`|Suitable for **OpenAI** style `embeddings` API, encoder-only model 
|`Extract Output Collections`|`3`|Suitable for **Voyage AI** style `contextualizedembeddings` API 

### llama.cpp stats 

|Model|Output Format|Embeddings per document|Embeddings per second|Seconds per document
|:-|-:|-:|-:|-:|
|Harrier OSS v1.0 0.6b|Text|`1`|`3.717`|`0.269`
|Harrier OSS v1.0 0.6b|Collection|`84`|`3.618`|`23.213`
|Harrier OSS v1.0 0.6b|Collection|`835`|`30.457`|`27.415`
|Granite Embedding English R2|Collection|`84`|`21.616`|`3.886`
|Granite Embedding English R2|Collection|`835`|`136.728`|`6.107`

#### Remarks

The sample `.docx` file has `835` semantic chunks, or paragraphs. 

A decoder-only model can generate `1` embedding for the entire document in `0.269` seconds. This strategy prioritised speed over relevance. The same decoder-only model can generate `1` embedding for each of the `835` semantic chunks in `27.415` seconds. Using a decoder-only model with small chunks is rather wasteful.

An encoder-only model can generate `84` embeddings with `10%` chunks each in `3.886` seconds. This strategy balances speed and relevance. The same encoder-only model can generate `1` embedding for each of the `835` semantic chunks in `6.107` seconds. This strategy is granular but lacks context awareness.
