/**
 * Example using NODEFS on a real local file system repository
 */
const lg = require('./libgit2.js');
lg.onRuntimeInitialized = () => {
    const FS = lg.FS;
    const NODEFS = FS.filesystems.NODEFS;
    
    // Open existing (libgit2) repo
    FS.mkdir('/working');
    FS.mount(NODEFS, { root: '../' }, '/working');
    FS.chdir('/working');
    jsgitinit();
    jsgitopenrepo();

    // Show git history
    jsgithistory();
    console.log(jsgithistoryresult);
    
};

