# 4d-plugin-extract
Universal Document Parser

## Abstract

The purpose of this plugin is to extract text from various document types in a format than be passed directly to one of the following endpoints:

- embeddings
- contextualizedembeddings

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
