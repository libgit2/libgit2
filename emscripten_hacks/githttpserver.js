/**
 * This example will create a git http server to a repository on your local disk.
 * Modify GIT_PROJECT_ROOT below to change the loction of your git repositories.
 */

const http = require('http');
const path = require('path');
const fs = require('fs');
const cgi = require('cgi');

const script = 'git';

const gitcgi = cgi(script, {args: ['http-backend'],
    stderr: process.stderr,
    env: {
        'GIT_PROJECT_ROOT': '/home/ubuntu/git',
        'GIT_HTTP_EXPORT_ALL': '1',
        'REMOTE_USER': 'hello@blabla.no' // Push requires authenticated users by default
    }
});

http.createServer( (request, response) => {
    
    let path = request.url.substring(1);
  
    if(path === '') {
        path = 'index.html';
    }
    
    console.log(request.url);
    if (request.url.indexOf('/gitrepos/') === 0 ) {
        request.url = request.url.substr('/gitrepos'.length);        
        gitcgi(request, response);
    } else if(fs.existsSync(path)) {
        if(path.indexOf('.js') === path.length-3) {
            response.setHeader('Content-Type', 'application/javascript');
        } else if(path.indexOf('.wasm') === path.length-3) {
            response.setHeader('Content-Type', 'application/wasm');
        }
        response.end(fs.readFileSync(path));
    } else {
        response.statusCode = 404;
        response.end('');
    }  
}).listen(5000);