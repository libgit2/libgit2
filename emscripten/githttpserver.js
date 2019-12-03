/**
 * This example will create a git http server to repositories on your local disk.
 * Set the GIT_PROJECT_ROOT environment variable to point to location of your git repositories.
 */

const http = require('http');
const path = require('path');
const fs = require('fs');
const cgi = require('cgi');

const script = 'git';

const gitcgi = cgi(script, {args: ['http-backend'],
    stderr: process.stderr,
    env: {
        'GIT_PROJECT_ROOT': process.env.GIT_PROJECT_ROOT,
        'GIT_HTTP_EXPORT_ALL': '1',
        'REMOTE_USER': 'test@example.com' // Push requires authenticated users by default
    }
});

http.createServer( (request, response) => {
    let path = request.url.substring(1);

    if(path === '') {
        path = 'index.html';
    }

    console.log(request.url);
    if(path.indexOf('lg2.') === 0) {
        // lg2.js and lg2.wasm are in the examples folder
        path = 'examples/' + path;
    }

    if(fs.existsSync(path)) {
        if(path.indexOf('.js') === path.length-3) {
            response.setHeader('Content-Type', 'application/javascript');
        } else if(path.indexOf('.wasm') === path.length-5) {
            response.setHeader('Content-Type', 'application/wasm');
        }
        response.end(fs.readFileSync(path));
    } else if( path.indexOf('git-upload') > -1 ||
        path.indexOf('git-receive') > -1) {  
        gitcgi(request, response);
    } else {
        response.statusCode = 404;
        response.end("not found");
    }
}).listen(5000);