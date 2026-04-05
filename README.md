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

- The architecture requires the `batch_size` to equal `max_position_embeddings` for long context.
- This is a distilled version of the `27b` model and uses the same tokenizer as `27b`.
- `F16` excels over `Q8_0` at short context but the margin narrows as the context get longer.

|Parameters|Dimensions|Context Length|Hidden Layers
|-:|-:|-:|-:
|`268098816`|`640`|`32768`|`18`

#### Q8_0

|Tokens|GPU Layers:0|GPU Layers:8|GPU Layers:16|GPU Layers:18
|-:|-:|-:|-:|-:|
|~`30000`|`32.5`|`25.0`|`18.5`|`15.6`
|~`10000`|`4.6`|`2.5`|`2.1`|`2.0`
|~`5000`|`2.6`|`1.6`|`1.0`|`0.8`
|~`1000`|`0.4`|`0.3`|`0.2`|`0.1`|`0.07`

#### F16

|Tokens|GPU Layers:0|GPU Layers:8|GPU Layers:16|GPU Layers:18
|-:|-:|-:|-:|-:|
|~`30000`|`30.0`|`21.5`|`19.5`|`14.9`
|~`10000`|`4.3`|`3.0`|`2.4`|`2.0`
|~`5000`|`2.5`|`0.8`|`0.6`|`0.5`
|~`1000`|`0.4`|`0.2`|`0.08`|`0.06`

- `15` seconds * `1` million documents = `173.61` days
- `1` second * `1` million documents = `11.57` days
- `0.1` seconds * `1` million documents = `1.15` days
- `0.05` seconds * `1` million documents = `13.89` hours

Even though the model supports up to `32768` tokens, the time required to generate embeddings increases **quadratically**. To generate a single token locally without stress, `10000` tokens is about the limit. To generate a batch of long context embeddings it is essential to **rent a GPU cluster**. Processing millions of document locally on a standard PC would take months.

## Example

```4d
var $query : Text

$q:="AI features integrated in 4D Write Pro"
$query:="Instruct: Retrieve text that relates to the topic\nTopic: "+$q

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
		
		var $AIReranker : cs.AIKit.Reranker
		$AIReranker:=cs.AIKit.Reranker.new({baseURL: "http://127.0.0.1:8081/v1"})
		var $RerankerParameters:=cs.AIKit.RerankerParameters.new({top_n: 5})
		
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
			Else 
				TRACE
			End if 
		End for each 
	End if 
End if 

$reranked:=$reranked.orderBy("score desc")

ALERT(JSON Stringify($reranked; *))
```

<img width="480" height="542" alt="Screenshot 2026-04-05 at 0 02 41" src="https://github.com/user-attachments/assets/accb11f4-d4f1-4efa-905f-36e02895d33b" />

<img width="480" height="542" alt="Screenshot 2026-04-05 at 0 03 29" src="https://github.com/user-attachments/assets/027266b6-34c3-46ef-9835-5fe73573b4f2" />

