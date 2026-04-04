//%attributes = {}
var $query : Text

$q:="AI features integrated in 4D Write Pro"
$query:="Instruct: Retrieve text that relates to the topic\nTopic: "+$q

var $AIClient : cs:C1710.AIKit.OpenAI
$AIClient:=cs:C1710.AIKit.OpenAI.new()
$AIClient.baseURL:="http://127.0.0.1:8080/v1"  // embeddings

var $batch : Object
$batch:=$AIClient.embeddings.create($query)

var $reranked : Collection
$reranked:=[]

If ($batch.success)
	$vector:=$batch.embedding.embedding
	var $comparison:={vector: $vector; metric: mk cosine:K95:1; threshold: 0.6}
	var $results:=ds:C1482.Documents.query("embeddings > :1"; $comparison)
	If ($results.length#0)
		
		ALERT:C41(JSON Stringify:C1217($results.text.extract("text").flat(); *))
		
		var $AIReranker : cs:C1710.AIKit.Reranker
		$AIReranker:=cs:C1710.AIKit.Reranker.new({baseURL: "http://127.0.0.1:8081/v1"})
		var $RerankerParameters:=cs:C1710.AIKit.RerankerParameters.new({top_n: 5})
		
		$documents:=$results.extract("ID"; "ID"; "text.text"; "documents")
		
		For each ($document; $documents)
			
			var $RerankerQuery:=cs:C1710.AIKit.RerankerQuery.new({\
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
				TRACE:C157
			End if 
		End for each 
	End if 
End if 

$reranked:=$reranked.orderBy("score desc")

ALERT:C41(JSON Stringify:C1217($reranked; *))