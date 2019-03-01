const lg = require('../libgit2_node.js');
lg.onRuntimeInitialized = () => {
    const FS = lg.FS;
    const MEMFS = FS.filesystems.MEMFS;
    
    // Create bare repo
    FS.mkdir('/working');
    FS.mount(MEMFS, { root: '.' }, '/working');
    FS.chdir('/working');
    jsgitinit();
    jsgitclone('https://github.com/fintechneo/browsergittestdata.git', 'testdata');
    console.log(jsgitgetlasterror());
    
    console.log(jsgitlasterrorresult);
    console.log(FS.chdir('testdata'));

    // destroy the config so that pull will fail
    const config = FS.readFile('.git/config', {encoding: 'utf8'});
    FS.writeFile('.git/config', '');
    
    jsgitpull();

    console.log(jsgitgetlasterror());
    console.log(jsgitlasterrorresult);

    if(jsgitlasterrorresult.klass===0) {
        throw new Error('Should be error after latest pull');
    }

    // repair git config
    FS.writeFile('.git/config', config);
    
    jsgitpull();

    console.log(jsgitgetlasterror());
    
    console.log(jsgitlasterrorresult);
    if(jsgitlasterrorresult.klass!==0) {
        throw new Error('Should be no error after latest pull');
    }
    
    jsgitpush();
    console.log(jsgitgetlasterror());
    
    console.log(jsgitlasterrorresult);
    if(jsgitlasterrorresult.klass===0) {
        throw new Error('Should fail to push since not logged in');
    }

}