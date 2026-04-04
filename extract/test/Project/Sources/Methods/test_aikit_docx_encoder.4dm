//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.docx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: 10}

/*
max_paragraph_length: n
each [] will contain up to n paragraphs
*/

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document DOCX; Extract Output Collection; $task)
$duration_extraction:=Abs:C99(Milliseconds:C459-$start_extraction)

If ($extracted.success)
	var $AIClient : cs:C1710.AIKit.OpenAI
	$AIClient:=cs:C1710.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1"
/*
embeddings
expect collection of text
*/
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs : Collection
	$start_embeddings:=Milliseconds:C459
	var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
	$batch:=$AIClient.embeddings.create($extracted.input)
	If ($batch.success)
		var $sentences : Collection
		$sentences:=$extracted.input
		var $embedding : Object
		For each ($embedding; $batch.embeddings)
			$vector:=$embedding.embedding
			$index:=$embedding.index  //sentence id in batch
			$text:=$sentences.at($index)
			$results.push({vector: $vector; text: $text})
		End for each 
	End if 
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

/*
granite
{
"time": "4.45 seconds total",
"count": 84,
"average": "18.876 embeddings per second"
}

harrier
{
"time": "23.213 seconds total",
"count": 84,
"average": "3.618 embeddings per second"
}
*/

ALERT:C41(JSON Stringify:C1217({\
time: String:C10(($duration_extraction+$duration_embeddings)/1000)+" seconds total"; \
count: $results.length; \
average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "####0.000")+" embeddings per second"}; *))