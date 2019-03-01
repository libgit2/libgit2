/**
 * Simple webserver with proxy to github.com
 */

const http = require('http');
const https = require('https');
const fs = require('fs');

function onRequest(request, response) {
    let path = request.url.substring(1);

    if(path.indexOf('/') > -1) {  
        const options = {
            hostname: 'github.com',
            port: 443,
            path: request.url,
            method: request.method
        };

        console.log(`Proxying ${options.method} request to ${options.hostname} with path ${options.path}`);
        const proxy = https.request(options, function (res) {
            res.pipe(response, {
            end: true
            });
        });

        request.pipe(proxy, {
            end: true
        });
    } else {
        if(path === '') {
            path = 'index.html';
        }
        
        if(fs.existsSync(path)) {
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
    }
}

http.createServer(onRequest).listen(5000);
