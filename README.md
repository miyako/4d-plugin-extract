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

#### `Extract Output Text`

- `input`: The entire document text concatenated.
- `documents`: The document divided into semantic chunks. Same as the `input` collection as `Extract Output Collection`

#### `Extract Output Collection`  

- `input`: The document divided into semantic chunks. Use `unique_values_only` and `max_paragraph_length` to control sampling rules.

#### `Extract Output Collections`  

- `inputs`: The document divided into chunks of semantic chunks. Use `unique_values_only` and `max_paragraph_length` to control sampling rules.

## Harrier OSS v1.0 230m

This is a distilled version of the `27b` model. 

|Parameters|Dimensions|Context Length|Hidden Layers
|-:|-:|-:|-:
|`268098816`|`640`|`32768`|`18`

#### Q8_0

|Tokens|GPU Layers:8|GPU Layers:16|GPU Layers:18
|-:|-:|-:|-:|
|~`30000`|`22.0`~`28.0`|`18.0`~`19.0`|`13.0`~`14.0`
|~`10000`|`2.0`~`3.0`|`1.8`~`2.6`|`1.6`~`2.4`
|~`5000`|`1.0`~`1.7`|`1.0`|`0.8`
|~`1000`|`0.1`~`0.3`|`0.1`|`0.07`

#### F16

|Tokens|GPU Layers:4|GPU Layers:8|GPU Layers:16|GPU Layers:18
|-:|-:|-:|-:|-:|
|~`30000`|`26.0`~`27.0`|`21.0`~`22.0`|`19.3`~`19.8`|`14.7`~`15.1`
|~`10000`|`3.4`~`4.4`|`2.6`~`3.5`|`2.2`~`2.5`|`1.8`~`2.3`
|~`5000`|`0.3`~`1.8`|`0.2`~`1.5`|`0.2`~`1.0`|`0.1`~`0.9`
|~`1000`|`0.1`~`0.2`|`0.16`~`0.19`|`0.05`~`0.11`|`0.04`~`0.08`

## Example

```4d
var $query : Text

$q:="Who talked about the latest 4D AI Kit feature?"
$query:="Instruct: Retrieve text that answers the query\nQuery: "+$q

var $AIClient : cs.AIKit.OpenAI
$AIClient:=cs.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

var $reranked : Collection
$reranked:=[]

If ($batch.success)
	$vector:=$batch.embedding.embedding
	var $comparison:={vector: $vector; metric: mk cosine; threshold: 0.6}
	var $results:=ds.Documents.query("embeddings > :1"; $comparison)
	If ($results.length#0)
		ALERT(JSON Stringify($results.text.extract("text").flat(); *))
	End if 
End if 
```

<img width="480" height="591" alt="Screenshot 2026-04-04 at 23 43 43" src="https://github.com/user-attachments/assets/abbe19a0-4aef-44cc-8834-93fff1b4f3c8" />

