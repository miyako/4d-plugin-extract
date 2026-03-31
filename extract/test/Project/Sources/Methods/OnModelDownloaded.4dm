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