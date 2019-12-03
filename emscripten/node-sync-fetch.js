const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
    const statusArray = new Int32Array(new SharedArrayBuffer(4));
    Atomics.store(statusArray, 0, 0);

    const resultBuffer = new SharedArrayBuffer(65536);
    const resultArray = new Uint8Array(resultBuffer);
    const worker = new Worker(__filename, {
        workerData: {
            statusArray: statusArray,
            resultArray: resultArray
        }
    }); 

    while(true) {
        Atomics.wait(statusArray, 0, 0);
        const length = statusArray[0];
        if(length === -1 ) {
            console.log('thats all');
            break;
        }
        
        Atomics.store(statusArray, 0, 0);
        Atomics.notify(statusArray, 0, 1);
        // console.log(new TextDecoder("utf-8").decode(resultArray.slice(0,length)));
        console.log(length);
    }
} else {
    const req = require('https').request('https://petersalomonsen.com',
        (res) => {
        res.on('data', chunk => {
            if(workerData.statusArray[0] !== 0) {
                Atomics.wait(workerData.statusArray, 0, workerData.statusArray[0]);
            }
            for(let n=0;n<chunk.length;n++) {
                workerData.resultArray[n] = chunk[n];
            }
            // console.log('chunk size ', chunk.length);
            Atomics.store(workerData.statusArray, 0, chunk.length);
            Atomics.notify(workerData.statusArray, 0, 1);
        });
        res.on('end', () => {
            Atomics.store(workerData.statusArray, 0, -1);
            Atomics.notify(workerData.statusArray, 0, 1);
        });
    });        
    // if(workerData.method === 'POST') {   
        // console.log(workerData.resultArray); 
        console.log(workerData.resultArray.slice(0, 4));        
         req.write(Buffer.from(workerData.resultArray.slice(0, 4)));
    // }
    
    req.end();
}
