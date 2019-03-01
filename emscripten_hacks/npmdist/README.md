WebAssembly port of [libgit2](https://libgit2.org)
==================================================

# Use in the web browser

Initialize a webworker in your html file:
```
<script>
const gitworker = new Worker("git_worker.js");
</script>
```

Copy `libgit2.js` and `libgit2.wasm` from the `node_modules/libgit2.js/` directory into the same folder as `git_worker.js`.

Then in the webworker file `git_worker.js`:

```
importScripts("libgit2.js");
Module['onRuntimeInitialized'] = () => {
    const dir="workdir";
    FS.mkdir(dir,"0777");
    FS.mount(IDBFS, {}, '/'+dir);
    FS.chdir("/"+dir);

    jsgitinit(); // Initialize the library

    jsgitclone("https://somehost/path-to-gitrepository.git","destinationfolder"); // clone a repository
    console.log(FS.readdir("destinationfolder")); // Read the cloned directory contents
};
```

# Use from nodejs

```
const lg = require('libgit2.js');
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