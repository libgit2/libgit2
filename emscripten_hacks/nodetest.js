const child_process = require('child_process');
let ret = 0;

ret |= child_process.spawnSync(process.argv0, ['./jstests/gitworktest.js'], {stdio: 'inherit'}).status ? 1 : 0;
ret |= child_process.spawnSync(process.argv0, ['./jstests/jsonmerge.js'], {stdio: 'inherit'}).status ? 1 : 0;
ret |= child_process.spawnSync(process.argv0, ['./jstests/nodejsclone.js'], {stdio: 'inherit'}).status ? 1 : 0;

process.exit(ret);
