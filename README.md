# Umka "CGI"

Umka "CGI" allows you to run Umka code as a "CGI" application.
(It's not CGI, but same purpose.)

## Usage

First run `umbox install`, you'll need [umbox](https://umbox.tophat2d.dev) to be installed.

Entry point is in `cgi-bin/main.um`.

You'll need to enable FFI in your `php.ini`.

On Windows, install MSVC and run `serve.bat <host>`.

On Linux, run `serve.sh <host>`.

## How it works

It works by routing PHP requests to Umka, through C FFI. Very stupid, but it's simpler than setting up a full CGI environment.

## Problems

If you load another PHP file it will show a PHP error.

Don't make another PHP file, otherwise the users will be able to navigate to it and possibly cause havoc.

In the future I'll get rid of PHP and just use C.
