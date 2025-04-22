<?php

class Request {
    public $headers;
    public $body;
}

class Response {
    public $code;
    public $headers;
    public $body;
}

class UmkaError {
    public $message;
    public $file;
    public $fn;
    public $line;
    public $column;
}

function _get_request() {
    $headers = "$_SERVER[REQUEST_METHOD] $_SERVER[REQUEST_URI] $_SERVER[SERVER_PROTOCOL]\n";
    $all_headers = getallheaders();
    foreach ($all_headers as $key => $value) {
        $headers .= "$key: $value\n";
    }
    $body = file_get_contents('php://input');

    $request = new Request();
    $request->headers = $headers;
    $request->body = $body;
    return $request;
}

function _ffi_load($libpath) {
    $ffi = FFI::cdef("
        typedef struct {
            const char* headers;
            const char* body;
        } request;

        typedef struct {
            int code;
            const char* headers;
            const char* body;
        } response;
        
        response umcgi_process(request req);
        void umcgi_free(response res);
    ", $libpath);

    return $ffi;
}

function _ffi_parse_error_string($str) {
    $len = intval($str);
    $message = "";
    $file = "";
    $fn = "";
    $line = 0;
    $column = 0;

    if ($len > 0) {
        $message = substr($str, strlen(strval($len))+1, $len);
        $skip = strlen(strval($len)) + $len + 1;
        $next = substr($str, $skip);
    
        $len = intval($next);
        $file = substr($next, strlen(strval($len))+1, $len);
        $skip = strlen(strval($len)) + $len + 1;
        $next = substr($next, $skip);
        
        $len = intval($next);
        $fn = substr($next, strlen(strval($len))+1, $len);
        $skip = strlen(strval($len)) + $len + 2;
        $next = substr($next, $skip);

        $line = intval($next);
        $skip = strlen(strval($line)) + 1;
        $next = substr($next, $skip);

        $column = intval($next);
    }

    $error = new UmkaError();
    $error->message = $message;
    $error->file = $file;
    $error->fn = $fn;
    $error->line = $line;
    $error->column = $column;
    return $error;
}

function _ffi_error_mnemonic($code) {
    $errors = [
        0 => "CODE_OK",
        1 => "CODE_ERROR_LOAD_DLL",
        2 => "CODE_ERROR_GET_PROC_ADDRESS_UMKAALLOC",
        3 => "CODE_ERROR_GET_PROC_ADDRESS_UMKAINIT",
        4 => "CODE_ERROR_ALLOC_UMKA",
        5 => "CODE_ERROR_INIT_UMKA",
        6 => "CODE_ERROR_COMPILE_UMKA",
        7 => "CODE_ERROR_GET_FUNC",
        8 => "CODE_ERROR_CALL_UMKA"
    ];
    return $errors[$code] ?? "UNKNOWN_ERROR";
}

function _ffi_process($ffi, Request $request) {
    $headers_len = strlen($request->headers);
    $body_len = strlen($request->body);

    $ffi_request = $ffi->new("request");
    $ffi_request->headers = FFI::new("char[$headers_len+1]", false);
    $ffi_request->body = FFI::new("char[$body_len+1]", false);
    FFI::memcpy($ffi_request->headers, $request->headers, $headers_len);
    FFI::memcpy($ffi_request->body, $request->body, $body_len);

    $ffi_response = $ffi->umcgi_process($ffi_request);

    $out_headers = FFI::string($ffi_response->headers);
    $out_body = FFI::string($ffi_response->body);

    FFI::free($ffi_request->headers);
    FFI::free($ffi_request->body);
    $ffi->umcgi_free($ffi_response);

    $response = new Response();
    $response->code = $ffi_response->code;
    $response->headers = $out_headers;
    $response->body = $out_body;

    return $response;
}

$ffi = null;
if (strncasecmp(PHP_OS, "WIN", 3) == 0) {
    $ffi = _ffi_load("umcgi/bus.dll");
} else {
    $ffi = _ffi_load("umcgi/bus.so");
}

$response = _ffi_process($ffi, _get_request());

if ($response->code != 0) {
    header("HTTP/1.1 500 Internal Server Error");

    $error = _ffi_parse_error_string($response->body);
    $error->message = htmlspecialchars($error->message);
    $error->file = htmlspecialchars($error->file);
    $error->fn = htmlspecialchars($error->fn);

    header("HTTP/1.1 500 Internal Server Error");
    header("Content-Type: text/html");
    echo "<pre style='color: red; font-family: monospace;'>";
    echo "Error: " . _ffi_error_mnemonic($response->code) . "\n";
    echo "\t{$error->file}:{$error->line}:{$error->column} ({$error->fn})\n";
    echo "\t{$error->message}\n";
    echo "</pre>";
    exit;
} else {
    $headers = explode("\n", $response->headers);

    foreach ($headers as $header) {
        header($header);
    }

    echo $response->body;
}

?>