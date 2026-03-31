//%attributes = {"invisible":true,"preemptive":"capable"}
#DECLARE($params : Object; $context : Object)

var $folder : 4D:C1709.Folder
var $file : 4D:C1709.File


Case of 
	: (OB Instance of:C1731($context; cs:C1710.event.models))
		
		//this branch is executed in preemptive process
		$folder:=$params.embeddings_model
		$file:=$folder.file($params.embeddings_model_name)
		
	Else 
		
		//this branch is executed in application process
		var $huggingface : cs:C1710.event.huggingface
		$huggingface:=$params.huggingfaces.huggingfaces.first()
		$folder:=$huggingface.folder
		$file:=$folder.file($huggingface.name)
		
End case 

If (Not:C34($file.exists))
	ALERT:C41("model does not exist!")
	return 
End if 

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

$status:=Embeddings Setup($file; $pooling)

If (Not:C34($status.success))
	ALERT:C41("failed to load model!")
End if 