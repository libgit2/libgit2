#!/usr/bin/env node

const markdownit = require('markdown-it');
const { program } = require('commander');
const minisearch = require('minisearch');

const path = require('node:path');
const fs = require('node:fs/promises');

const linkPrefix = '/docs/reference';

const defaultBranch = 'main';

function uniqueifyId(api, nodes) {
    let suffix = "", i = 1;

    while (true) {
        const possibleId = `${api.kind}-${api.name}${suffix}`;
	let collision = false;

        for (const item of nodes) {
	    if (item.id === possibleId) {
	        collision = true;
	        break;
	    }
        }

	if (!collision) {
	    return possibleId;
	}

	suffix = `-${++i}`;
    }
}

async function produceSearchIndex(version, apiData) {
    const nodes = [ ];

    for (const group in apiData['groups']) {
        for (const name in apiData['groups'][group]['apis']) {
            const api = apiData['groups'][group]['apis'][name];

	    let displayName = name;

	    if (api.kind === 'macro') {
	      displayName = displayName.replace(/\(.*/, '');
	    }

            const apiSearchData = {
	        id: uniqueifyId(api, nodes),
                name: displayName,
		group: group,
                kind: api.kind
            };

            apiSearchData.description = Array.isArray(api.comment) ?
	        api.comment[0] : api.comment;

            let detail = "";

            if (api.kind === 'macro') {
                detail = api.value;
            }
            else if (api.kind === 'alias') {
                detail = api.type;
            }
            else {
                let details = undefined;

                if (api.kind === 'struct' || api.kind === 'enum') {
                    details = api.members;
                }
                else if (api.kind === 'function' || api.kind === 'callback') {
                    details = api.params;
                }
                else {
                    throw new Error(`unknown api type '${api.kind}'`);
                }

                for (const item of details || [ ]) {
                    if (detail.length > 0) {
                        detail += ' ';
                    }

                    detail += item.name;

                    if (item.comment) {
                        detail += ' ';
                        detail += item.comment;
                    }
                }

                if (api.kind === 'function' || api.kind === 'callback') {
                    if (detail.length > 0 && api.returns?.type) {
                        detail += ' ' + api.returns.type;
                    }

                    if (detail.length > 0 && api.returns?.comment) {
                        detail += ' ' + api.returns.comment;
                    }
                }
            }

            detail = detail.replaceAll(/\s+/g, ' ')
                           .replaceAll(/[\"\'\`]/g, '');

            apiSearchData.detail = detail;

            nodes.push(apiSearchData);
        }
    }

    const index = new minisearch({
	fields: [ 'name', 'description', 'detail' ],
	storeFields: [ 'name', 'group', 'kind', 'description' ],
	searchOptions: { boost: { name: 5, description: 2 } }
    });

    index.addAll(nodes);

    const filename = `${outputPath}/${version}.json`;
    await fs.mkdir(outputPath, { recursive: true });
    await fs.writeFile(filename, JSON.stringify(index, null, 2));
}

function versionSort(a, b) {
    if (a === b) {
        return 0;
    }

    const aVersion = a.match(/^v(\d+)(?:\.(\d+)(?:\.(\d+)(?:\.(\d+))?)?)?(?:-(.*))?$/);
    const bVersion = b.match(/^v(\d+)(?:\.(\d+)(?:\.(\d+)(?:\.(\d+))?)?)?(?:-(.*))?$/);

    if (!aVersion && !bVersion) {
        return a.localeCompare(b);
    }
    else if (aVersion && !bVersion) {
        return -1;
    }
    else if (!aVersion && bVersion) {
        return 1;
    }

    for (let i = 1; i < 5; i++) {
        if (!aVersion[i] && !bVersion[i]) {
            break;
        }
        else if (aVersion[i] && !bVersion[i]) {
            return 1;
        }
        else if (!aVersion[i] && bVersion[i]) {
            return -1;
        }
        else if (aVersion[i] !== bVersion[i]) {
            return aVersion[i] - bVersion[i];
        }
    }

    if (aVersion[5] && !bVersion[5]) {
        return -1;
    }
    else if (!aVersion[5] && bVersion[5]) {
        return 1;
    }
    else if (aVersion[5] && bVersion[5]) {
        return aVersion[5].localeCompare(bVersion[5]);
    }

    return 0;
}

program.option('--verbose')
       .option('--version <version...>');
program.parse();

const options = program.opts();

if (program.args.length != 2) {
    console.error(`usage: ${path.basename(process.argv[1])} raw_api_dir output_dir`);
    process.exit(1);
}

const docsPath = program.args[0];
const outputPath = program.args[1];

(async () => {
    try {
        const v = options.version ? options.version :
            (await fs.readdir(docsPath))
                     .filter(a => a.endsWith('.json'))
                     .map(a => a.replace(/\.json$/, ''));

        const versions = v.sort(versionSort).reverse();

	for (const version of versions) {
            if (options.verbose) {
                console.log(`Reading documentation data for ${version}...`);
            }

            const apiData = JSON.parse(await fs.readFile(`${docsPath}/${version}.json`));

            if (options.verbose) {
                console.log(`Creating minisearch index for ${version}...`);
            }

            await produceSearchIndex(version, apiData);
        }
    } catch (e) {
        console.error(e);
        process.exit(1);
    }
})();
