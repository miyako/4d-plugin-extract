//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.xlsx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: 10}

If (True:C214)
	//to extract with document structure
	$extracted_o:=Extract(Extract Document XLSX; Extract Output Object; $task)
	//to extract raw text 
	$extracted_t:=Extract(Extract Document XLSX; Extract Output Text; $task)
End if 

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document XLSX; Extract Output Collection; $task)
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
	For each ($input; $extracted.inputs)
		var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
		$batch:=$AIClient.embeddings.create($input)
		If ($batch.success)
			var $sentences : Collection
			$sentences:=$input
			var $embedding : Object
			For each ($embedding; $batch.embeddings)
				$vector:=$embedding.embedding
				$index:=$embedding.index  //sentence id in batch
				$text:=$sentences.at($index)
				$results.push({vector: $vector; text: $text})
			End for each 
		End if 
	End for each 
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

/*
{
"count": 71,
"average": "161.7 embeddings per second"
}
*/

ALERT:C41(JSON Stringify:C1217({\
count: $results.length; \
average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "#####.0")+" embeddings per second"}; *))