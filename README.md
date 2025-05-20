# UmCGI

UmCGI is FastCGI bindings for Umka.

## Usage

You need to have these things installed:
- [umbox](https://umbox.tophat2d.dev)
- `libfcgi-dev`
- `spawn-fcgi`
- `nginx`

1. First run `umbox install`
2. Run `./compile.sh` to compile the program.
3. Use `spawn-fcgi` to run it as a FastCGI application.

```
spawn-fcgi -a 127.0.0.1 -p 9000 -- umcgi.cgi
```

4. Put the main file at `cgi-bin/main.um` relative to the cwd of the cgi process.
5. Route nginx requests to `127.0.0.1:9000`. Go to `/etc/nginx/sites-available/default` and add or edit inside `server` block:

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

Tip: restart nginx if it doesn't work `sudo nginx -s reload`

Tip: If that didn't work either, try `sudo service nginx restart`

## API

It's currently basic, you can get the headers and fastcgi_params through `getenv`. You can read the whole body with `getbody` and write chunks of data with `write`.

```rs
// Writes output to response boody
fn write*(data: []uint8): uint
// Reads one character from request body, -1 for EOF
fn getchar*(): int
// Returns headers and Nginx parameters
fn getenv*(): []str
// Reads whole request body, affected by `getchar`, and returns empty array on subsequent calls.
fn getbody*(): []uint8 
```

## Example program

```
import "fcgi.um"

fn write_string(s: str) {
    fcgi::write([]uint8(s))
}

fn main() {
    write_string("Content-Type: text/html\r\n\r\n")
    body := str(fcgi::getbody())
    envs := fcgi::getenv()
    write_string(sprintf("Body:<pre>%llv</pre><hr>Headers:<pre>%llv</pre>", body, envs))
}
```
