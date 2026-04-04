//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.docx")

$task:={file: $file}

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document DOCX; Extract Output Text; $task)
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
	$input:=$extracted.input
	var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
	$batch:=$AIClient.embeddings.create($input)
	If ($batch.success)
		$results.push({vector: $batch.embedding.embedding; text: $input})
	End if 
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

/*
after the 1st request
{
"time": "0.269 seconds total",
"count": 1,
"average": "3.717 embeddings per second"
}
*/

ALERT:C41(JSON Stringify:C1217({\
time: String:C10(($duration_extraction+$duration_embeddings)/1000)+" seconds total"; \
count: $results.length; \
average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "####0.000")+" embeddings per second"}; *))