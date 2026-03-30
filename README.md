# 4d-plugin-extract
Universal Document Parser

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
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs : Collection
	$start_embeddings:=Milliseconds
	For each ($page; $extracted.pages)
		$paragraphs:=$page.inputs  //paragraphs in page
		var $paragraph : Collection
		For each ($paragraph; $paragraphs)
			var $batch : Object
			$batch:=Embeddings({input: $paragraph}; Embedding Simple)
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

ALERT(JSON Stringify({extraction: $duration_extraction; embeddings: $duration_embeddings; count: $results.length}; *))
```

## Contextualized Embeddings

_Each sentence is prefixed with paragraph context before vectorization._

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
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs; $inputs : Collection
	var $len; $pos : Integer
	$start_embeddings:=Milliseconds
	For each ($page; $extracted.pages)
		$paragraphs:=$page.inputs  //paragraphs in page
		$len:=9  //number of paragraphs to process 
		$pos:=0  //slicing offset
		$inputs:=$paragraphs.slice($pos; $pos+$len)
		var $batch : Object
		While ($inputs.length#0)
			$batch:=Embeddings({input: $inputs}; Embedding Contextual)
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

ALERT(JSON Stringify({extraction: $duration_extraction; embeddings: $duration_embeddings; count: $results.length}; *))
```
