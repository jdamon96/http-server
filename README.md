# http-server
An http server implemented in C that can serve static HTML and image files.

**Usage**:

Starting the program: ./http-server <server_port> <web_root>

For example, ./http-server 8888  ̃/html should serve an index.html to the following request: http://the.machine.the.server.is.running.on:8888/index.html

**Details**:

• Only supports GET method
• Strictly HTTP 1.0 server (will accept GET requests that are eitehr HTTP/1.0 or 1.1, but always responds with HTTP/1.0)
