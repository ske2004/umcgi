# UmCGI

UmCGI is FastCGI bindings for Umka.

## Usage

First run `umbox install`, you'll need [umbox](https://umbox.tophat2d.dev) to be installed.

Run `./compile.sh` to compile the program.

Use `spawn-fcgi` to run it as a FastCGI application.

```
spawn-fcgi -a 127.0.0.1 -p 9000 -- umcgi.cgi
```

Put the main file at `cgi-bin/main.um` relative to the cwd of the cgi process.

Route nginx requests to `127.0.0.1:9000`. Go to `/etc/nginx/sites-available/default` and add inside `server` block :

```
location / {
    fastcgi_pass 127.0.0.1:9000;
    fastcgi_param QUERY_STRING    $query_string;
    fastcgi_param REQUEST_METHOD  $request_method;
    fastcgi_param CONTENT_TYPE    $content_type;
    fastcgi_param CONTENT_LENGTH  $content_length;
    fastcgi_param SCRIPT_FILENAME $request_uri;
}
```

Tip: restart nginx if it doens't work `sudo nginx -s reload`

## Example program

```
import "fcgi.um"

fn write_string(s: str) {
    fcgi::FCGI_Write([]uint8([]char(s)))
}

fn read_whole_body(): str {
    input := []uint8{}
    for true {
        c := fcgi::FCGI_GetChar()
        if c == -1 {
            break
        }
        input = append(input, c)
    }

    return str([]char(input))
}

fn get_env(): []str {
    return fcgi::FCGI_GetEnv(typeptr([]str))
}

fn main() {
    write_string("Content-Type: text/html\r\n\r\n")
    body := read_whole_body()
    envs := get_env()
    write_string(sprintf("Body:<pre>%llv</pre><hr>Headers:<pre>%llv</pre>", body, envs))
}
```
