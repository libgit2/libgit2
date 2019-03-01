const https = require('https');
async function main() {
    const data = await new Promise(resolve => process.stdin.on('data', data => resolve(data)));
    let headerendindex = data.indexOf('\r\n\r\n');
    let headerlengthincrement = 4;
    if(headerendindex === -1) {
        headerendindex = data.indexOf('\n\n');
        headerlengthincrement = 2;
    }
    
    const headers = data.toString('utf-8',0, headerendindex).split('\n');
    
    const firstlineparts = headers[0].split(' ');
    const method = firstlineparts[0];
    const path = firstlineparts[1];
    
    const host = headers.find(header => header.indexOf('Host:') === 0)
                        .substr('Host:'.length).trim();
                         
    await new Promise(resolve => {
        const req = https.request({
            host: host,        
            method: method,
            path: path,
            headers: headers.slice(1).map(header => header.split(': ').map(c => c.trim()))
        }, (res) => {        
            res.on('data', (responsedata) => process.stdout.write(responsedata)); 
        });        
        if(method === 'POST') {            
            req.write(data.slice(headerendindex + headerlengthincrement));
        }
        req.end();
    });
}
main();

    