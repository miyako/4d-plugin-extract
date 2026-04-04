//%attributes = {"invisible":true,"preemptive":"capable"}
TRUNCATE TABLE:C1051([Documents:1])
SET DATABASE PARAMETER:C642([Documents:1]; Table sequence number:K37:31; 0)

var $file : 4D:C1709.File
var $extracted : Object

$files:=Folder:C1567("/RESOURCES/docx").files(fk ignore invisible:K87:22 | fk recursive:K87:7)\
.query("extension == :1"; ".docx")

var $e : cs:C1710.DocumentsEntity

For each ($file; $files)
	
	$task:={\
		file: $file; \
		unique_values_only: True:C214; \
		max_paragraph_length: 10}
	
	$extracted:=Extract(Extract Document DOCX; Extract Output Text; $task)
	
	If ($extracted.success)
		$e:=ds:C1482.Documents.new()
/*
alternatively you could hit 
http://localhost:8080/tokenize
and let llama-server tokenize 
but here we just want an approximation 
*/
		ARRAY TEXT:C222($words; 0)
		GET TEXT KEYWORDS:C1141($extracted.input; $words)
		$e.embeddings:=$batch.embedding.embedding
		$e.text:={text: $extracted.documents}
		$e.count:=Size of array:C274($words)
		$e.input:=$extracted.input
		$e.save()
	End if 
	
End for each 

var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"

For each ($e; ds:C1482.Documents.all().orderBy("count asc"))
	var $page : Object
	var $paragraphs; $documents : Collection
	$input:=$e.input
	$documents:=$e.text.documents
	$start:=Milliseconds:C459
	var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
	$batch:=$AIClient.embeddings.create($input)
	$duration:=(Milliseconds:C459-$start)/1000
	If ($batch.success)
		$e.embeddings:=$batch.embedding.embedding
		$e.duration:=(?00:00:00?)+$duration
		$e.count:=$batch.usage.prompt_tokens
		$e.input:=Null:C1517
	Else 
		$e.error:=$batch.errors.extract("body.error.message").join("\r")
	End if 
	$e.save()
End for each 