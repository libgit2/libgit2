#!/usr/bin/env node

const fs = require('fs');
const process = require('node:process');

const path = process.argv[2] || '.';
const output = process.argv[3];

const dirs = fs.readdirSync(path);

tests = [ ];
results = { };

for (const dir of dirs) {
    const name = dir.replace(/^results-/, '');

    const control = JSON.parse(fs.readFileSync(`${path}/${dir}/control.json`));
    const candidate = JSON.parse(fs.readFileSync(`${path}/${dir}/candidate.json`));

    results[name] = { };

    const controlTests = control.tests.map((x) => x.name);
    const candidateTests = candidate.tests.map((x) => x.name);
    tests = [...new Set(tests.concat(controlTests).concat(candidateTests))];

    for (const test of tests) {
        const controlResults = control.tests.filter((x) => x.name === test)[0];
        const candidateResults = candidate.tests.filter((x) => x.name === test)[0];

        let result;

        if (controlResults.results.status === 'ok' &&
            candidateResults.results.status === 'ok') {
            const delta = (candidateResults.results.mean - controlResults.results.mean) / candidateResults.results.mean;
            const deltaPercent = Math.round(Math.abs(delta) * 1000) / 10;
            const direction = (delta > 0) ? 'slower' : 'faster';
            let highlight = '';

            if (delta > 0.1) {
                highlight = '**';
            }

            results[name][test] = `${highlight}${deltaPercent}% ${direction}${highlight}`;
        }
        else if (controlResults.results.status === 'ok') {
            results[name][test] = `${candidateResults.results.stauts} in candidate`;
        }
        else if (candidateResults.results.status === 'ok') {
            results[name][test] = `${controlResults.results.stauts} in control`;
        }
        else {
            results[name][test] = controlResults.results.status;
        }
    }
}

let markdown = '| Platform \\ Test |';

for (const test in Object.values(results)[0]) {
    markdown += ` ${test} |`;
}
markdown += '\n';

markdown += `| --- |`;
for (const test in Object.values(results)[0]) {
    markdown += ` --- |`;
}
markdown += '\n';

for (const name in results) {
    markdown += `| ${name.replaceAll(/-/g, '&#x2011;')} |`;

    for (const test in results[name]) {
        markdown += ` ${results[name][test]} |`;
    }

    markdown += `\n`;
}

if (output) {
    fs.writeFileSync(output, markdown);
}
else {
    console.log(markdown);
}

function p(arr, q) {
    const array = [...arr].sort();
    const pos = (array.length - 1) * q;
    const base = Math.floor(pos);
    const rest = pos - base;

    if (array[base + 1] !== undefined) {
        return array[base] + rest * (array[base + 1] - array[base]);
    }
    else {
        return array[base];
    }
}
