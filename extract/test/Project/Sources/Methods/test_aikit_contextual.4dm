//%attributes = {"invisible":true}
var $file : 4D:C1709.File
var $extracted : Object

$file:=File:C1566("/RESOURCES/sample.xlsx")
//$file:=Folder(fk desktop folder).file("D100022700.xlsx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: -1}
//$task:={file: $file.getContent()}

$start_extraction:=Milliseconds:C459
$extracted:=Extract(Extract Document XLSX; Extract Output Collection; $task)
$duration_extraction:=Abs:C99(Milliseconds:C459-$start_extraction)

If ($extracted.success)
	var $AIClient : cs:C1710.AIKit.OpenAI
	$AIClient:=cs:C1710.AIKit.OpenAI.new()
	$AIClient.baseURL:="http://127.0.0.1:8080/v1/contextualized"
	var $results : Collection
	$results:=[]
	var $page : Object
	var $paragraphs; $inputs : Collection
	var $len; $pos : Integer
	$start_embeddings:=Milliseconds:C459
	For each ($page; $extracted.pages)
		$paragraphs:=$page.inputs  //paragraphs in page
		$len:=4  //number of paragraphs to process 
		$pos:=0  //slicing offset
		$inputs:=$paragraphs.slice($pos; $pos+$len)
		var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
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
	$duration_embeddings:=Abs:C99(Milliseconds:C459-$start_embeddings)
End if 

ALERT:C41(JSON Stringify:C1217({\
extraction: $duration_extraction; \
embeddings: $duration_embeddings; \
count: $results.length; average: String:C10($results.length/(($duration_extraction+$duration_embeddings)/1000); "#####.0")+" embeddings per second"}; *))