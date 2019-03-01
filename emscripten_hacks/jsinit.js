/**
 * replace jsgitprogresscallback with your own progress message handler.
 */
jsgitprogresscallback = function(progressmessage) {
    console.log(progressmessage);
}

jsgitinit = cwrap('jsgitinit', null, []);
jsgitclone = cwrap('jsgitclone', null, ['string', 'string']);
jsgitinitrepo = cwrap('jsgitinitrepo', null, ['number']);
jsgitopenrepo = cwrap('jsgitopenrepo', null, []);
jsgitadd = cwrap('jsgitadd', null, ['string']);
jsgitsetuser = cwrap('jsgitsetuser', null, ['string', 'string']);
jsgitresolvemergecommit = cwrap('jsgitresolvemergecommit', null, []);
jsgitremove = cwrap('jsgitremove', null, ['string']);
jsgitworkdirnumberofdeltas = cwrap('jsgitworkdirnumberofdeltas', 'number', []);
jsgitstatus = cwrap('jsgitstatus', 'number', []);
jsgitaddfileswithchanges = cwrap('jsgitaddfileswithchanges', null, []);
jsgitpush = cwrap('jsgitpush', null, []);
jsgitpull = cwrap('jsgitpull', null, []);
jsgitreset_hard = cwrap('jsgitreset_hard', null, ['string']);
jsgitshutdown = cwrap('jsgitshutdown', null, []);
jsgitprintlatestcommit = cwrap('jsgitprintlatestcommit', null, []);
jsgitcommit = cwrap('jsgitcommit', null, ['string']);
jsgithistory = cwrap('jsgithistory', null, []);
jsgitregisterfilter = cwrap('jsgitregisterfilter', null, ['string', 'string', 'number']);
jsgitgetlasterror = cwrap('jsgitgetlasterror', null, ['number']);

const nodePermissions = FS.nodePermissions;
FS.nodePermissions = function(node, perms) { 
    if(node.mode & 0o100000) {
        /* 
         * Emscripten doesn't support the sticky bit, while libgit2 sets this on some files.
         * grant permission if sticky bit is set
         */        
        return 0;
    } else {
        return nodePermissions(node, perms);
    }
};