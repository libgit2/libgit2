/**
 * Stream for use with emscripten in nodejs. Uses https in child process for synchronous requests
 * 
 * To use: git_stream_register_tls(git_open_emscripten_node_stream);
 * 
 * Module.jsgitheaders = [{name: 'Authorization', value: 'Bearer TOKEN'}]
 * 
 * Author: Peter Johan Salomonsen ( https://github.com/petersalomonsen ) 
 */
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "streams/stransport.h"

git_stream xhrstream;

int emscripten_connect(git_stream *stream) {
	printf("Connecting\n");	
	EM_ASM(
		gitxhrdata = null;
	);
	return 0;
}

ssize_t emscripten_read(git_stream *stream, void *data, size_t len) {
	size_t ret = 0;
		
	EM_ASM_({		
		if(gitxhrdata!==null) {			
			const cp = require('child_process');
			
			console.log((Buffer.from(gitxhrdata)+'').substr(0, 500));
			const proc = cp.spawnSync(process.argv0, [__dirname + '/libgit2httpsrequest.js'], {input: 
				gitxhrdata
			});
			gitxhrdata = null;
			responsedata = proc.stdout;	
			console.log('response', (responsedata+'').substr(0, 500));		
		}		
	});
		
	EM_ASM_({
		const availlen = (responsedata.length-gitxhrreadoffset);						
		const len = availlen > $2 ? $2 : availlen;
						
		const byteArray = responsedata.slice(gitxhrreadoffset, gitxhrreadoffset + len);		
		console.log("read from ",responsedata.length,gitxhrreadoffset,len,byteArray.length);
		writeArrayToMemory(byteArray,$0);
		setValue($1,len,"i32");
		
		gitxhrreadoffset+=len;						
	},data,&ret,len);	
	printf("Returning %d bytes, requested %d\n",ret,len);
	return ret;
}

int emscripten_certificate(git_cert **out, git_stream *stream) {
	printf("Checking certificate\n");
	return 0;
}

ssize_t emscripten_write(git_stream *stream, const char *data, size_t len, int flags) {
	EM_ASM_({
		const data = new Uint8Array(Module.HEAPU8.buffer, $0, $1);
		const method = UTF8ToString($0, 4).trim();
		
		if(method === 'GET') {						
			gitxhrreadoffset = 0;
			const cp = require('child_process');			
			// console.log(Buffer.from(data)+'');
			const proc = cp.spawnSync(process.argv0, [__dirname + '/libgit2httpsrequest.js'], {input: 
				data
			});			
			responsedata = proc.stdout;				
		} else if(method === 'POST') {	
			responsedata = null;			
			gitxhrreadoffset = 0;
			gitxhrdata = data.slice(0);
		} else {			
			const appended = new Uint8Array(gitxhrdata.length+$1);
			appended.set(gitxhrdata,0);
			appended.set(new Uint8Array(Module.HEAPU8.buffer,$0,$1),gitxhrdata.length);
			gitxhrdata = appended;										
			console.log("Appended post data",$1,gitxhrdata.length);	
		}
	},data,len);
	
	return len;
}

int emscripten_close(git_stream *stream) {
	printf("Close\n");
	return 0;
}

void emscripten_free(git_stream *stream) {
	printf("Free\n");
	//git__free(stream);
}

int git_open_emscripten_nodejs_stream(git_stream **out, const char *host, const char *port) {		
	xhrstream.version = GIT_STREAM_VERSION;
	xhrstream.connect = emscripten_connect;
	xhrstream.read = emscripten_read;
	xhrstream.write = emscripten_write;
	xhrstream.close = emscripten_close;
	xhrstream.free = emscripten_free;
	xhrstream.certificate = emscripten_certificate;
	xhrstream.encrypted = 1;
	xhrstream.proxy_support = 0;
		
	*out = &xhrstream;
	printf("Stream setup \n");
	return 0;
}

#endif
