var $llama : cs:C1710.llama.llama
var $huggingfaces : cs:C1710.event.huggingfaces
var $embeddings; $rerank : cs:C1710.event.huggingface

var $homeFolder : 4D:C1709.Folder
$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".ONNX")

var $file : 4D:C1709.File
var $URL : Text
var $port : Integer

var $event : cs:C1710.event.event
$event:=cs:C1710.event.event.new()

$event.onError:=Formula:C1597(OnModelDownloaded)
$event.onSuccess:=Formula:C1597(OnModelDownloaded)

$event.onData:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; This:C1470.file.fullName+":"+String:C10((This:C1470.range.end/This:C1470.range.length)*100; "###.00%")))
//$event.onData:=Formula(MESSAGE(This.file.fullName+":"+String((This.range.end/This.range.length)*100; "###.00%")))
$event.onResponse:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; This:C1470.file.fullName+":download complete"))
//$event.onResponse:=Formula(MESSAGE(This.file.fullName+":download complete"))
$event.onTerminate:=Formula:C1597(LOG EVENT:C667(Into 4D debug message:K38:5; (["process"; $1.pid; "terminated!"].join(" "))))

$homeFolder:=Folder:C1567(fk home folder:K87:24).folder(".GGUF")
var $max_position_embeddings; $batch_size; $parallel; $threads; $batches : Integer

Case of 
	: (True:C214)  //decoder
		
		$folder:=$homeFolder.folder("harrier-oss-v1-0.6b")
		$path:="harrier-oss-v1-0.6b-Q8_0.gguf"
		$URL:="keisuke-miyako/harrier-oss-v1-0.6b-gguf-q8_0"
		
		$max_position_embeddings:=32768
		$pooling:="last"
		
	: (False:C215)  //encoder
		
		$folder:=$homeFolder.folder("granite-embedding-english-r2")
		$path:="granite-embedding-english-r2-Q8_0.gguf"
		$URL:="keisuke-miyako/granite-embedding-english-r2-gguf-q8_0"
		
		$max_position_embeddings:=8192
		$pooling:="cls"
		
End case 

$batch_size:=$max_position_embeddings
$ubatch_size:=2048
$batches:=1
$threads:=8  // M1 Pro P-cores

var $logFile : 4D:C1709.File
$logFile:=$folder.file("llama.log")
$folder.create()
If (Not:C34($logFile.exists))
	$logFile.setContent(4D:C1709.Blob.new())
End if 

$port:=8080
$options:={\
embeddings: True:C214; \
pooling: $pooling; \
log_file: $logFile; \
ctx_size: $max_position_embeddings*$batches; \
batch_size: $batch_size; \
ubatch_size: 2048; \
parallel: $batches; \
threads: $threads; \
threads_batch: $threads; \
threads_http: 2; \
log_disable: False:C215; \
n_gpu_layers: 0}

$embeddings:=cs:C1710.event.huggingface.new($folder; $URL; $path)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$embeddings])
$llama:=cs:C1710.llama.llama.new($port; $huggingfaces; $homeFolder; $options; $event)

//

$folder:=$homeFolder.folder("granite-embedding-reranker-english-r2")
$path:="granite-embedding-reranker-english-r2-Q8_0.gguf"
$URL:="keisuke-miyako/granite-embedding-reranker-english-r2-gguf-q8_0"

$max_position_embeddings:=8192
$pooling:="rank"

$batch_size:=$max_position_embeddings
$ubatch_size:=8192
$batches:=1
$threads:=8  // M1 Pro P-cores

$port:=8081
$options:={\
embeddings: True:C214; \
pooling: $pooling; \
log_file: $logFile; \
fit: "on"; \
ctx_size: $max_position_embeddings*$batches; \
batch_size: $batch_size; \
ubatch_size: $ubatch_size; \
parallel: $batches; \
threads: $threads; \
threads_batch: $threads; \
threads_http: 2; \
log_disable: False:C215; \
n_gpu_layers: -1}

$rerank:=cs:C1710.event.huggingface.new($folder; $URL; $path)
$huggingfaces:=cs:C1710.event.huggingfaces.new([$rerank])
$llama:=cs:C1710.llama.llama.new($port; $huggingfaces; $homeFolder; $options; $event)