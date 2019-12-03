# Building libgit2 with emscripten

Using [emscripten](https://emscripten.org) (minimum version 1.39.6) you may compile libgit2 to webassembly and run it in nodejs or in a web browser.

The script in [build.sh](build.sh) shows how to configure and build, and you'll find the resulting lg2.js/lg2.wasm under the generated `examples` folder.

An example of interacting with libgit2 from nodejs can be found in [example_node.js](example_node.js).

An example for the browser (using webworkers) can be found in [example_webworker.js](example_webworker.js). You can start a webserver for this by running the [webserverwithgithubproxy.js](webserverwithgithubproxy.js) script, which will launch a http server at http://localhost:5000 with a proxy to github. Proxy instead of direct calls is needed because of CORS restrictions in a browser environment.

This is dependent on mmap fixes in emscripten:

- https://github.com/emscripten-core/emscripten/pull/10095
- https://github.com/emscripten-core/emscripten/pull/10526