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
|docx|`Extract Document XLSX`|`1`|
|pptc|`Extract Document XLSX`|`2`|

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
|Granite Embedding English R2|Collection|`84`|`18.876`|`4.45`

### ONNX Runtime stats 

|Model|Output Format|Embeddings per document|Embeddings per second|Seconds per document
|:-|-:|-:|-:|-:|
|Granite Embedding English R2|Collection|`84`|`5.874`|`14.298`


## Simple Embeddings 

_Each sentence is vectorized independently._

```4d
var $file : 4D.File
var $extracted : Object

$file:=File("/RESOURCES/sample.xlsx")

$task:={\
file: $file; \
unique_values_only: True; \
max_paragraph_length: -1}
//$task:={file: $file.getContent()}

$start_extraction:=Milliseconds
$extracted:=Extract(Extract Document XLSX; Extract Output Collection; $task)
$duration_extraction:=Abs(Milliseconds-$start_extraction)

If ($extracted.success)
	var $AIClient : cs.AIKit.OpenAI
	$AIClient:=cs.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1"
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs : Collection
	$start_embeddings:=Milliseconds
	For each ($page; $extracted.pages)
		$paragraphs:=$page.inputs  //paragraphs in page
		var $paragraph : Collection
		For each ($paragraph; $paragraphs)
			var $batch : cs.AIKit.OpenAIEmbeddingsResult
			$batch:=$AIClient.embeddings.create($paragraph)
			//result is 1 vector per sentence in paragraph, no context
			If ($batch.success)
				var $sentences : Collection
				$sentences:=$paragraph.flat()
				var $embedding : Object
				For each ($embedding; $batch.embeddings)
					$vector:=$embedding.embedding
					$index:=$embedding.index  //sentence id in batch
					$text:=$sentences.at($index)
					$results.push({vector: $vector; text: $text})
				End for each 
			End if 
		End for each 
	End for each 
	$duration_embeddings:=Abs(Milliseconds-$start_embeddings)
End if 

ALERT(JSON Stringify({\
extraction: $duration_extraction; \
embeddings: $duration_embeddings; \
count: $results.length; average: String($results.length/(($duration_extraction+$duration_embeddings)/1000); "#####.0")+" embeddings per second"}; *))
```

## Contextualized Embeddings

_Each sentence is prefixed with paragraph context before vectorisation._

```4d
var $file : 4D.File
var $extracted : Object

$file:=File("/RESOURCES/sample.xlsx")

$task:={\
file: $file; \
unique_values_only: True; \
max_paragraph_length: -1}
//$task:={file: $file.getContent()}

$start_extraction:=Milliseconds
$extracted:=Extract(Extract Document XLSX; Extract Output Collection; $task)
$duration_extraction:=Abs(Milliseconds-$start_extraction)

If ($extracted.success)
	var $AIClient : cs.AIKit.OpenAI
	$AIClient:=cs.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1/contextualized"
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs; $inputs : Collection
	var $len; $pos : Integer
	$start_embeddings:=Milliseconds
	For each ($page; $extracted.pages)
		$paragraphs:=$page.inputs  //paragraphs in page
		$len:=4  //number of paragraphs to process 
		$pos:=0  //slicing offset
		$inputs:=$paragraphs.slice($pos; $pos+$len)
		var $batch : cs.AIKit.OpenAIEmbeddingsResult
		While ($inputs.length#0)
			$batch:=$AIClient.embeddings.create($inputs)
			//result is 1 vector per sentence, where context=paragraph
			If ($batch.success)
				var $sentences : Collection
				$sentences:=$inputs.flat()
				var $embedding : Object
				For each ($embedding; $batch.embeddings)
					$vector:=$embedding.embedding
					$index:=$embedding.index  //sentence id in batch
					$text:=$sentences.at($index)
					$results.push({vector: $vector; text: $text})
				End for each 
			End if 
			$pos+=$len
			$inputs:=$paragraphs.slice($pos; $pos+$len)
		End while 
	End for each 
	$duration_embeddings:=Abs(Milliseconds-$start_embeddings)
End if 

ALERT(JSON Stringify({\
extraction: $duration_extraction; \
embeddings: $duration_embeddings; \
count: $results.length; average: String($results.length/(($duration_extraction+$duration_embeddings)/1000); "#####.0")+" embeddings per second"}; *))
```

The scope of each `inputs` array and its contents are arbitrarty. It can be sentence-paragraph, paragraph-page, or even page-document.
