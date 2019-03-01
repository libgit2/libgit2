Build for webassembly using Emscripten
======================================

This is an example of exposing basic operations of libgit2 to javascript for use in the browser and nodejs. The library is compiled to webassembly using [emscripten](https://emscripten.org)

# Build using Emscripten

First of all you need to source the emscripten sdk from where emscripten is located on your system:

    source ./emsdk_env.sh

Then from this folder, run the shell script:

    bash build.sh

When complete you should see the files `libgit2.js` / `libgit2.wasm` and `libgit2_node.js` / `libgit2_node.wasm`.

# Build using Docker

If you're not able to install emscripten, there's a simple alternative using docker (which is also used in the Azure build pipeline).

Simply from the repository root directory, run:

`docker run --rm -v $(pwd):/src trzeci/emscripten bash emscripten_hacks/build.sh`

# Use in the browser
Because of CORS restrictions in the browser you cannot read from github directly from another domain. You need to add a proxy on your web server. You can run the githttpproxy.js script in this folder to 
get a local webserver with proxy to github.com:

    node githttpproxy.js

Navigate your browser to `http://localhost:5000`

When testing with the index.html file included here you should open the web console which will prompt "ready" when loaded. Remember to switch to the libgit2 webworker for typing commands in the console.

Type the commands:

    jsgitinit();
    jsgitclone("https://github.com/pathto/mygitrepo.git","mygitrepo");

You'll see the git clone process starts. This is not a small repository so it takes some time.

When the clone is done you can list the folder contents by typing:

    FS.readdir("mygitrepo")

A simple demonstration video can be seen here: https://youtu.be/rcBluzpUWE4

## Why is a webworker needed?

http requests from libgit2 are synchronous, and operations such as clone / pull  / push may take some times which would hang if run on the browser main thread. WebWorkers support both synchronous http requests and also working excellent for long running operations.

# Use in nodejs

You can also use the library from nodejs. The node version is different since it uses the nodejs HTTP API. Also synchronous HTTP is not straight forward with node, so this is solved by running a separate process for HTTP and call this using `execSync`.

```
const lg = require('libgit2_node.js');
lg.onRuntimeInitialized = () => {
    const FS = lg.FS;
    const MEMFS = FS.filesystems.MEMFS;

    FS.mkdir('/working');
    FS.mount(MEMFS, { root: '.' }, '/working');
    FS.chdir('/working');

    jsgitinit(); // Initialize the library
    jsgitclone("https://somehost/path-to-gitrepository.git","destinationfolder"); // clone a repository
    console.log(FS.readdir("destinationfolder")); // Read the cloned directory contents
};
```