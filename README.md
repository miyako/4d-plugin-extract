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

### llama.cpp stats 

#### Decoder-only Embeddings

|Model|Output Format|Embeddings per document|Embeddings per second|Seconds per document
|:-|-:|-:|-:|-:|
|Harrier OSS v1.0 0.6b|Text|`1`|`3.717`|`116.023`
|Harrier OSS v1.0 0.6b|Collection|`84`|`3.618`|`23.213`
|Harrier OSS v1.0 0.6b|Collection|`835`|`30.457`|`27.415`

#### Encoder-only Embeddings

|Model|Output Format|Embeddings per document|Embeddings per second|Seconds per document
|:-|-:|-:|-:|-:|
|Granite Embedding English R2|Collection|`84`|`21.616`|`3.886`
|Granite Embedding English R2|Collection|`835`|`136.728`|`6.107`

#### Remarks

The sample `.docx` file has `835` semantic chunks, or paragraphs. The total number of tokens is `18776`.

A decoder-only model can generate `1` embedding for the entire document in `116.023` seconds if the model and graph are not fully loaded in memory. It is important to not offload to the GPU unless you have plenty of memory (`40GB` or more). It is also important to pad the text so that `llama-server` does not have to builds a new GGML computation graph per unique sequence length. Since `qwen3` models have a `32768` context length, it makes sense to have buckets of `512`, `1024`, `2048`, `4096`, `8192`, `16384`, and `32767`. The longest sequence (`32767`) takes `352` seconds, or `6` minutes on the first call, but the next could be fast as `0.298` seconds. Without buckets, the embedding phase may need `100000000` minutes to process the same number of documents, i.e. more than `10` months.

This strategy prioritises speed over relevance. The same decoder-only model can generate `1` embedding for each of the `835` semantic chunks, or  `84` embeddings with up to `100` chunks each in about the same time. It is more efficient to create `1` embedding for the whole document.

An encoder-only model can generate `84` embeddings with up to `100` chunks each in `3.886` seconds, or `1` embedding for each of the `835` semantic chunks in `6.107` seconds. Smaller chnuks are faster to process but results is more embeddings will less context. You need to strike the right balance between speed and relevance.

## Suggestion

Use a ModernBERT encoder-only model with `8192` context length (e.g. Granite Embedding English R2) to generate embeddings in chunks.

Use a Qwen decoder-only model with `32768` context length (e.g. Harrier OSS v1.0 0.6b) to generate a single embedding for the entire document.

For the best of both worlds, use a reranker model with `8192` context length (e.g. e.g. Granite Embedding Reranker English R2).

## Example

- harrier-oss-v1-0.6b-Q8_0.gguf

`llama-server` will consume about `7GB` of memory and `800%` CPU at peak inference.

```4d
$batches:=1
$threads:=8 
$max_position_embeddings:=32768
$pooling:="last"
```

<img width="740" height="384" alt="Screenshot 2026-04-04 at 13 42 40" src="https://github.com/user-attachments/assets/801c6bc2-f768-40cd-86c3-4452d3cf3af5" />

- granite-embedding-reranker-english-r2-Q8_0.gguf

`llama-server` consumes about `150MB` of memory.

```4d
$batches:=1
$threads:=8 
$max_position_embeddings:=8192
$pooling:="rank"
```

<img width="740" height="384" alt="Screenshot 2026-04-04 at 14 49 13" src="https://github.com/user-attachments/assets/b0da23a3-886f-48ee-8327-2e747dc5a4fc" />


```4d
var $query : Text

$q:="Intissar and Sara talks about the latest AI Kit feature"
$query:="Instruct: Retrieve text that matches the passage\nPassage: "+$q

$q:="Thibaud and Josh talks about the future of 4D"
$query:="Instruct: Retrieve text that matches the passage\nPassage: "+$q

var $AIClient : cs.AIKit.OpenAI
$AIClient:=cs.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

var $reranked : Collection
$reranked:=[]

If ($batch.success)
	$vector:=$batch.embedding.embedding
	var $comparison:={vector: $vector; metric: mk cosine; threshold: 0.5}
	var $results:=ds.Documents.query("embeddings > :1"; $comparison)
	If ($results.length#0)
		
		var $AIReranker : cs.AIKit.Reranker
		$AIReranker:=cs.AIKit.Reranker.new({baseURL: "http://127.0.0.1:8081/v1"})
		var $RerankerParameters:=cs.AIKit.RerankerParameters.new({top_n: 6})
		
		$documents:=$results.extract("ID"; "ID"; "text.text"; "documents")
		
		For each ($document; $documents)
			
			var $RerankerQuery:=cs.AIKit.RerankerQuery.new({\
			query: $query; \
			documents: $document.documents})
			
			$batch:=$AIReranker.rerank.create($RerankerQuery; $RerankerParameters)
			If ($batch.success)
				For each ($result; $batch.results)
					$reranked.push({\
					ID: $document.ID; \
					score: $result.relevance_score; \
					text: $document.documents.at($result.index)})
				End for each 
			End if 
		End for each 
	End if 
End if 

$reranked:=$reranked.orderBy("relevance_score desc").slice(0; 3)

ALERT(JSON Stringify($reranked; *))
```

<img width="480" height="542" alt="Screenshot 2026-04-04 at 14 53 09" src="https://github.com/user-attachments/assets/7e7d2282-032d-48f0-b0f6-536de9783a87" />
<img width="480" height="542" alt="Screenshot 2026-04-04 at 14 52 50" src="https://github.com/user-attachments/assets/c947632f-7d43-4f05-982b-b148496b11cd" />

