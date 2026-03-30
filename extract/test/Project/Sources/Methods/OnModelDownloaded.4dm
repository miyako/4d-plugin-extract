//%attributes = {"invisible":true,"preemptive":"capable"}
#DECLARE($params : Object; $models : cs:C1710.event.models)

var $homeFolder : 4D:C1709.Folder
$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".ONNX")
$folder:=$homeFolder.folder("granite-embedding-english-r2")

var $folder : 4D:C1709.Folder
$folder:=$params.embeddings_model
var $file : 4D:C1709.File
$file:=$folder.file($params.embeddings_model_name)

ALERT:C41(JSON Stringify:C1217({path: $file.path; pooling: $params.pooling}))

var $pooling : Integer

Case of 
	: (Value type:C1509($params.pooling)#Is text:K8:3)
		$pooling:=Extract Pooling Mean
	: ($params.pooling="mean")
		$pooling:=Extract Pooling Mean
	: ($params.pooling="cls")
		$pooling:=Extract Pooling CLS
	: ($params.pooling="multi-vector")
		$pooling:=Extract Pooling Multi Vector
	: ($params.pooling="last-token")
		$pooling:=Extract Pooling Last Token
	: ($params.pooling="E2E")
		$pooling:=Extract Pooling E2E
	Else 
		$pooling:=Extract Pooling Mean
End case 

Embeddings Setup($file; $pooling)