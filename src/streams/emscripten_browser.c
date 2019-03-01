/**
 * Stream for use with emscripten in the browser. Makes use of XmlHttpRequest
 * 
 * To use: git_stream_register_tls(git_open_emscripten_stream);
 * 
 * If you need to access another domain, you should set the Module.jsgithost to e.g. "https://somegitdomain.com"
 * You can also add custom headers by setting the Module.jsgitheaders. Example:
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
	
	unsigned int readyState = 0;
	EM_ASM_({		
		if(gitxhrdata!==null) {
			console.log("sending post data",gitxhrdata.length);
			gitxhr.send(gitxhrdata.buffer);			
			gitxhrdata = null;
		} 
		setValue($0,gitxhr.readyState,"i32");
	},&readyState);
	
	/*
	 * We skip this since we are now using a synchronous request
	while(readyState!=4) {
		EM_ASM_({
			console.log("Waiting for data");
			setValue($0,gitxhr.readyState,"i32");
		},&readyState);
		
		emscripten_sleep(10);
	}*/
	
	EM_ASM_({
		if(gitxhr) {
			var arrayBuffer = gitxhr.response; // Note: not oReq.responseText
					
			if (gitxhr.readyState===4 && arrayBuffer) {		
				var availlen = (arrayBuffer.byteLength-gitxhrreadoffset);						
				var len = availlen > $2 ? $2 : availlen;
								
				var byteArray = new Uint8Array(arrayBuffer,gitxhrreadoffset,len);		
				//console.log("read from ",arrayBuffer.byteLength,gitxhrreadoffset,len,byteArray[0]);
				writeArrayToMemory(byteArray,$0);
				setValue($1,len,"i32");
				
				gitxhrreadoffset+=len;				
			}
		} else {
			setValue($1,-1,"i32");
		}
	},data,&ret,len);	
	//printf("Returning %d bytes, requested %d\n",ret,len);
	return ret;
}

int emscripten_certificate(git_cert **out, git_stream *stream) {
	printf("Checking certificate\n");
	return 0;
}

ssize_t emscripten_write(git_stream *stream, const char *data, size_t len, int flags) {
	EM_ASM_({
		var data = UTF8ToString($0);
		
        var host = Module.jsgithost ? Module.jsgithost : '';
		var headers = Module.jsgitheaders ? Module.jsgitheaders : [];
		function addHeaders() {
			for(var n=0; n<headers.length; n++) {
				gitxhr.setRequestHeader(headers[n].name, headers[n].value);
			}
		}

		if(data.indexOf("GET ")===0) {				
			gitxhr=new XMLHttpRequest();
			gitxhrreadoffset = 0;
			gitxhr.responseType = "arraybuffer";			
			gitxhr.open("GET",host + data.split("\n")[0].split(" ")[1], false);		
			addHeaders();
			gitxhr.send();
		} else if(data.indexOf("POST ")===0) {
			gitxhr=new XMLHttpRequest();
			gitxhrreadoffset = 0;
			gitxhr.responseType = "arraybuffer";			
			var requestlines = data.split("\n");			
			gitxhr.open("POST", host + requestlines[0].split(" ")[1], false);
			addHeaders();
			console.log(data);
			gitxhrdata = null;								
			for(var n=1;n<requestlines.length;n++) {
				if(requestlines[n].indexOf("Content-Type")===0) {
					gitxhr.setRequestHeader("Content-Type",requestlines[n].split(": ")[1].trim());
				}	
			}			
		} else {
			if(gitxhrdata===null) {				
				console.log("New post data",$1,data);
				gitxhrdata = new Uint8Array($1);
				gitxhrdata.set(new Uint8Array(Module.HEAPU8.buffer,$0,$1),0);				
			} else {
				var appended = new Uint8Array(gitxhrdata.length+$1);
				appended.set(gitxhrdata,0);
				appended.set(new Uint8Array(Module.HEAPU8.buffer,$0,$1),gitxhrdata.length);
				gitxhrdata = appended;										
				console.log("Appended post data",$1,gitxhrdata.length,data);
			}
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

int git_open_emscripten_stream(git_stream **out, const char *host, const char *port) {		
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
