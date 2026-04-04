//%attributes = {"invisible":true,"preemptive":"capable"}
TRUNCATE TABLE:C1051([Documents:1])
SET DATABASE PARAMETER:C642([Documents:1]; Table sequence number:K37:31; 0)

var $file : 4D:C1709.File
var $extracted : Object

$files:=Folder:C1567("/RESOURCES/docx").files(fk ignore invisible:K87:22 | fk recursive:K87:7)\
.query("extension == :1"; ".docx")

$task:={\
file: $file; \
unique_values_only: True:C214; \
max_paragraph_length: 10}

$extracted:=Extract(Extract Document DOCX; Extract Output Text; $task)

For each ($file; $files)
	
	$task:={\
		file: $file; \
		unique_values_only: True:C214; \
		max_paragraph_length: 10}
	
	$extracted:=Extract(Extract Document DOCX; Extract Output Text; $task)
	
	If ($extracted.success)
		var $AIClient : cs:C1710.AIKit.OpenAI
		$AIClient:=cs:C1710.AIKit.OpenAI.new()
		$AIClient.baseURL:="http://127.0.0.1:8080/v1"
		var $page : Object
		var $paragraphs; $documents : Collection
		$input:=$extracted.input
		$documents:=$extracted.documents
		$start:=Milliseconds:C459
		var $batch : cs:C1710.AIKit.OpenAIEmbeddingsResult
		$batch:=$AIClient.embeddings.create($input)
		$duration:=(Milliseconds:C459-$start)/1000
		If ($batch.success)
			var $e : cs:C1710.DocumentsEntity
			$e:=ds:C1482.Documents.new()
			$e.embeddings:=$batch.embedding.embedding
			$e.text:={text: $documents}
			$e.duration:=(?00:00:00?)+$duration
			$e.save()
		End if 
	End if 
End for each 