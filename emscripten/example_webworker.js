importScripts('lg2.js');

Module.onRuntimeInitialized = () => {
    const lg = Module;

    FS.mkdir('/working');
    FS.mount(MEMFS, { }, '/working');
    FS.chdir('/working');    

    FS.writeFile('/home/web_user/.gitconfig', '[user]\n' +
                'name = Test User\n' +
                'email = test@example.com');

    // clone a local git repository and make some commits

    lg.callMain(['clone','http://localhost:5000/test', 'testrepo']);
    FS.chdir('testrepo');
    FS.writeFile('test.txt', 'hello');
    
    lg.callMain(['add', '--verbose', 'test.txt']);
    lg.callMain(['commit','-m','test 123']);
    
    lg.callMain(['log']);
    lg.callMain(['status']);

    
    lg.callMain(['status']);

    FS.writeFile('test.txt', 'second revision');

    lg.callMain(['add', 'test.txt']);

    lg.callMain(['status']);
    lg.callMain(['commit','-m','test again']);

    lg.callMain(['status']);

    lg.callMain(['log']);

    lg.callMain(['push']);
};