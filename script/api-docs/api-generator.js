#!/usr/bin/env node

const path = require('node:path');
const child_process = require('node:child_process');
const fs = require('node:fs').promises;
const util = require('node:util');
const process = require('node:process');

const { program } = require('commander');

const includePath = (p) => `${p}/include`;
const ancientIncludePath = (p) => `${p}/src/git`;
const legacyIncludePath = (p) => `${p}/src/git2`;
const standardIncludePath = (p) => `${includePath(p)}/git2`;
const systemIncludePath = (p) => `${includePath(p)}/git2/sys`;

const fileIgnoreList = [ 'stdint.h', 'inttypes.h' ];
const apiIgnoreList = [ 'GIT_BEGIN_DECL', 'GIT_END_DECL', 'GIT_WIN32' ];

// Some older versions of libgit2 need some help with includes
const defaultIncludes = [
    'checkout.h', 'common.h', 'diff.h', 'email.h', 'oidarray.h', 'merge.h', 'remote.h', 'types.h'
];

// We're unable to fully map `types.h` defined types into groups;
// provide some help.
const groupMap = {
    'filemode': 'tree',
    'treebuilder': 'tree',
    'note': 'notes',
    'packbuilder': 'pack',
    'reference': 'refs',
    'push': 'remote' };

async function headerPaths(p) {
    const possibleIncludePaths = [
        ancientIncludePath(p),
        legacyIncludePath(p),
        standardIncludePath(p),
        systemIncludePath(p)
    ];

    const includePaths = [ ];
    const paths = [ ];

    for (const possibleIncludePath of possibleIncludePaths) {
        try {
            await fs.stat(possibleIncludePath);
            includePaths.push(possibleIncludePath);
        }
        catch (e) {
            if (e?.code !== 'ENOENT') {
                throw e;
            }
        }
    }

    if (!includePaths.length) {
        throw new Error(`no include paths for ${p}`);
    }

    for (const fullPath of includePaths) {
        paths.push(...(await fs.readdir(fullPath)).
            filter((filename) => filename.endsWith('.h')).
            filter((filename) => !fileIgnoreList.includes(filename)).
            filter((filename) => filename !== 'deprecated.h' || !options.deprecateHard).
            map((filename) => `${fullPath}/${filename}`));
    }

    return paths;
}

function trimPath(basePath, headerPath) {
    const possibleIncludePaths = [
        ancientIncludePath(basePath),
        legacyIncludePath(basePath),
        standardIncludePath(basePath),
        systemIncludePath(basePath)
    ];

    for (const possibleIncludePath of possibleIncludePaths) {
        if (headerPath.startsWith(possibleIncludePath + '/')) {
            return headerPath.substr(possibleIncludePath.length + 1);
        }
    }

    throw new Error("header path is not beneath include root");
}

function parseFileAst(path, ast) {
    let currentFile = undefined;
    const fileData = [ ];

    for (const node of ast.inner) {
        if (node.loc?.file && currentFile != node.loc.file) {
            currentFile = node.loc.file;
        } else if (node.loc?.spellingLoc?.file && currentFile != node.loc.spellingLoc.file) {
            currentFile = node.loc.spellingLoc.file;
        }

        if (currentFile != path) {
            continue;
        }

        fileData.push(node);
    }

    return fileData;
}

function includeBase(path) {
    const segments = path.split('/');

    while (segments.length > 1) {
        if (segments[segments.length - 1] === 'git2' ||
            segments[segments.length - 1] === 'git') {
            segments.pop();
            return segments.join('/');
        }

        segments.pop();
    }

    throw new Error(`could not resolve include base for ${path}`);
}

function readAst(path, options) {
    return new Promise((resolve, reject) => {
        let errorMessage = '';
        const chunks = [ ];

        const processArgs = [ path, '-Xclang', '-ast-dump=json', `-I${includeBase(path)}` ];

        if (options?.deprecateHard) {
            processArgs.push(`-DGIT_DEPRECATE_HARD`);
        }

        if (options?.includeFiles) {
            for (const file of options.includeFiles) {
                processArgs.push(`-include`);
                processArgs.push(file)
            }
        }

        const process = child_process.spawn('clang', processArgs);

        process.stderr.on('data', (message) => {
            errorMessage += message;
        });
        process.stdout.on('data', (chunk) => {
            chunks.push(chunk);
        });
        process.on('close', (code) => {
            if (code != 0 && options.strict) {
                reject(new Error(`clang exit code ${code}: ${errorMessage}`));
            }
            else if (code != 0) {
                resolve([ ]);
            }
            else {
                const ast = JSON.parse(Buffer.concat(chunks).toString());
                resolve(parseFileAst(path, ast));
            }
        });
        process.on('error', function (err) {
            reject(err);
        });
    });
}

async function readFile(path) {
    const buf = await fs.readFile(path);
    return buf.toString();
}

function ensure(message, test) {
    if (!test) {
        throw new Error(message);
    }
}

function ensureDefined(name, value) {
    if (!value) {
        throw new Error(`could not find ${name} for declaration`);
    }

    return value;
}

function groupifyId(location, id) {
    if (!id) {
        throw new Error(`could not find id in declaration`);
    }

    if (!location || !location.file) {
        throw new Error(`unspecified location`);
    }

    return `${location.file}-${id}`;
}

function blockCommentText(block) {
    ensure('block does not have a single paragraph element', block.inner.length === 1 && block.inner[0].kind === 'ParagraphComment');
    return commentText(block.inner[0]);
}

function richBlockCommentText(block) {
    ensure('block does not have a single paragraph element', block.inner.length === 1 && block.inner[0].kind === 'ParagraphComment');
    return richCommentText(block.inner[0]);
}

function paramCommentText(param) {
    ensure('param does not have a single paragraph element', param.inner.length === 1 && param.inner[0].kind === 'ParagraphComment');
    return richCommentText(param.inner[0]);
}

function appendCommentText(chunk) {
    return chunk.startsWith('    ') ? "\n" + chunk : chunk;
}

function commentText(para) {
    let text = '';

    for (const comment of para.inner) {
        // docbook allows backslash escaped text, and reports it differently.
        // we restore the literal `\`.
        if (comment.kind === 'InlineCommandComment') {
            text += `\\${comment.name}`;
        }
        else if (comment.kind === 'TextComment') {
            text += text ? "\n" + comment.text : comment.text;
        } else {
            throw new Error(`unknown paragraph comment element: ${comment.kind}`);
        }
    }

    return text.trim();
}

function nextText(para, idx) {
    if (!para.inner[idx + 1] || para.inner[idx + 1].kind !== 'TextComment') {
        throw new Error("expected text comment");
    }

    return para.inner[idx + 1].text;
}

function inlineCommandData(data, command) {
    ensure(`${command} information does not follow @${command}`, data?.kind === 'TextComment');

    const result = data.text.match(/^(?:\[([^\]]+)\])? ((?:[a-zA-Z0-9\_]+)|`[a-zA-Z0-9\_\* ]+`)(.*)/);
    ensure(`${command} data does not follow @${command}`, result);

    const [ , attr, spec, remain ] = result;
    return [ attr, spec.replace(/^`(.*)`$/, "$1"), remain ]
}

function richCommentText(para) {
    let text = '';
    let extendedType = undefined;
    let subkind = undefined;
    let versionMacro = undefined;
    let initMacro = undefined;
    let initFunction = undefined;
    let lastComment = undefined;

    for (let i = 0; i < para.inner?.length; i++) {
        const comment = para.inner[i];

        if (comment.kind === 'InlineCommandComment' &&
            comment.name === 'type') {
            const [ attr, data, remain ] = inlineCommandData(para.inner[++i], "type");

            extendedType = { kind: attr, type: data };
            text += remain;
        }
        else if (comment.kind === 'InlineCommandComment' &&
            comment.name === 'flags') {
            subkind = 'flags';
        }
        else if (comment.kind === 'InlineCommandComment' &&
            comment.name === 'options') {
            const [ attr, data, remain ] = inlineCommandData(para.inner[++i], "options");

            if (attr === 'version') {
                versionMacro = data;
            }
            else if (attr === 'init_macro') {
                initMacro = data;
            }
            else if (attr === 'init_function') {
                initFunction = data;
            }

            subkind = 'options';
            text += remain;
        }
        // docbook allows backslash escaped text, and reports it differently.
        // we restore the literal `\`.
        else if (comment.kind === 'InlineCommandComment') {
            text += `\\${comment.name}`;
        }
        else if (comment.kind === 'TextComment') {
            // clang oddity: it breaks <things in brackets> into two
            // comment blocks, assuming that the trailing > should be a
            // blockquote newline sort of thing. unbreak them.
            if (comment.text.startsWith('>') &&
                lastComment &&
                lastComment.loc.offset + lastComment.text.length === comment.loc.offset) {

                text += comment.text;
            } else {
                text += text ? "\n" + comment.text : comment.text;
            }
        }
        else if (comment.kind === 'HTMLStartTagComment' && comment.name === 'p') {
            text += "\n";
        }
        else {
            throw new Error(`unknown paragraph comment element: ${comment.kind}`);
        }

        lastComment = comment;
    }

    return {
        text: text.trim(),
        extendedType: extendedType,
        subkind: subkind,
        versionMacro: versionMacro,
        initMacro: initMacro,
        initFunction: initFunction
    }
}

function join(arr, elem) {
    if (arr) {
        return [ ...arr, elem ];
    }

    return [ elem ];
}

function joinIfNotEmpty(arr, elem) {
    if (!elem || elem === '') {
        return arr;
    }

    if (arr) {
        return [ ...arr, elem ];
    }

    return [ elem ];
}

function pushIfNotEmpty(arr, elem) {
    if (elem && elem !== '') {
        arr.push(elem);
    }
}

function single(arr, fn, message) {
    let result = undefined;

    if (!arr) {
        return undefined;
    }

    for (const match of arr.filter(fn)) {
        if (result) {
            throw new Error(`multiple matches in array for ${fn}${message ? ' (' + message + ')': ''}`);
        }

        result = match;
    }

    return result;
}

function updateLocation(location, decl) {
    location.file = trimBase(decl.loc?.spellingLoc?.file || decl.loc?.file) || location.file;
    location.line = decl.loc?.spellingLoc?.line || decl.loc?.line || location.line;
    location.column = decl.loc?.spellingLoc?.col || decl.loc?.col || location.column;

    return location;
}

async function readFileLocation(startLocation, endLocation) {
    if (startLocation.file != endLocation.file) {
        throw new Error("cannot read across files");
    }

    const data = await fs.readFile(startLocation.file, "utf8");
    const lines = data.split(/\r?\n/).slice(startLocation.line - 1, endLocation.line);

    lines[lines.length - 1] = lines[lines.length - 1].slice(0, endLocation.column);
    lines[0] = lines[0].slice(startLocation.column - 1);

    return lines
}

function formatLines(lines) {
    let result = "";
    let continuation = false;

    for (const i in lines) {
        if (!continuation) {
            lines[i] = lines[i].trimStart();
        }

        continuation = lines[i].endsWith("\\");

        if (continuation) {
            lines[i] = lines[i].slice(0, -1);
        } else {
            lines[i] = lines[i].trimEnd();
        }

        result += lines[i];
    }

    if (continuation) {
        throw new Error("unterminated literal continuation");
    }

    return result;
}

async function parseExternalRange(location, range) {
    const startLocation = {...location};
    startLocation.file = trimBase(range.begin.spellingLoc.file || startLocation.file);
    startLocation.line = range.begin.spellingLoc.line || startLocation.line;
    startLocation.column = range.begin.spellingLoc.col || startLocation.column;

    const endLocation = {...startLocation};
    endLocation.file = trimBase(range.end.spellingLoc.file || endLocation.file);
    endLocation.line = range.end.spellingLoc.line || endLocation.line;
    endLocation.column = range.end.spellingLoc.col || endLocation.column;

    const lines = await readFileLocation(startLocation, endLocation);

    return formatLines(lines);
}

async function parseLiteralRange(location, range) {
    const startLocation = updateLocation({...location}, { loc: range.begin });
    const endLocation = updateLocation({...location}, { loc: range.end });

    const lines = await readFileLocation(startLocation, endLocation);

    return formatLines(lines);
}

async function parseRange(location, range) {
    return range.begin.spellingLoc ? parseExternalRange(location, range) : parseLiteralRange(location, range);
}

class ParserError extends Error {
    constructor(message, location) {
        if (!location) {
            super(`${message} at (unknown)`);
        }
        else {
            super(`${message} at ${location.file}:${location.line}`);
        }
        this.name = 'ParserError';
    }
}

function validateParsing(test, message, location) {
    if (!test) {
        throw new ParserError(message, location);
    }
}

function parseComment(spec, location, comment, options) {
    let result = { };
    let last = undefined;

    for (const c of comment.inner.filter(c => c.kind === 'ParagraphComment' || c.kind === 'VerbatimLineComment')) {
        if (c.kind === 'ParagraphComment') {
            const commentData = richCommentText(c);

            result.comment = joinIfNotEmpty(result.comment, commentData.text);
            delete commentData.text;

            result = { ...result, ...commentData };
        }
        else if (c.kind === 'VerbatimLineComment') {
            result.comment = joinIfNotEmpty(result.comment, c.text.trim());
        }
        else {
            throw new Error(`unknown comment ${c.kind}`);
        }
    }

    for (const c of comment.inner.filter(c => c.kind !== 'ParagraphComment' && c.kind !== 'VerbatimLineComment')) {
        if (c.kind === 'BlockCommandComment' && c.name === 'see') {
            result.see = joinIfNotEmpty(result.see, blockCommentText(c));
        }
        else if (c.kind === 'BlockCommandComment' && c.name === 'note') {
            result.notes = joinIfNotEmpty(result.notes, blockCommentText(c));
        }
        else if (c.kind === 'BlockCommandComment' && c.name === 'deprecated') {
            result.deprecations = joinIfNotEmpty(result.deprecations, blockCommentText(c));
        }
        else if (c.kind === 'BlockCommandComment' && c.name === 'warning') {
            result.warnings = joinIfNotEmpty(result.warnings, blockCommentText(c));
        }
        else if (c.kind === 'BlockCommandComment' &&
                 (c.name === 'return' || (c.name === 'returns' && !options.strict))) {
            const returnData = richBlockCommentText(c);

            result.returns = {
                extendedType: returnData.extendedType,
                comment: returnData.text
            };
        }
        else if (c.kind === 'ParamCommandComment') {
            ensure('param has a name', c.param);

            const paramDetails = paramCommentText(c);

            result.params = join(result.params, {
                name: c.param,
                direction: c.direction,
                values: paramDetails.type,
                extendedType: paramDetails.extendedType,
                comment: paramDetails.text
            });
        }
        else if (options.strict) {
            if (c.kind === 'BlockCommandComment') {
                throw new ParserError(`unknown block command comment ${c.name}`, location);
            }
            else if (c.kind === 'VerbatimBlockComment') {
                throw new Error(`unknown verbatim command comment ${c.name}`, location);
            }
            else {
                throw new Error(`unknown comment ${c.kind} in ${kind}`);
            }
        }
    }

    return result;
}

async function parseFunction(location, decl, options) {
    let result = {
        kind: 'function',
        id: groupifyId(location, decl.id),
        name: ensureDefined('name', decl.name),
        location: {...location}
    };

    // prototype
    const [ , returnType, ] = decl.type.qualType.match(/(.*?)(?: )?\((.*)\)$/) || [ ];
    ensureDefined('return type declaration', returnType);
    result.returns = { type: returnType };

    for (const paramDecl of decl.inner.filter(attr => attr.kind === 'ParmVarDecl')) {
        updateLocation(location, paramDecl);

        const inner = paramDecl.inner || [];
        const innerLocation = {...location};
        let paramAnnotations = undefined;

        for (const annotateDecl of inner.filter(attr => attr.kind === 'AnnotateAttr')) {
            updateLocation(innerLocation, annotateDecl);

            paramAnnotations = join(paramAnnotations, await parseRange(innerLocation, annotateDecl.range));
        }

        result.params = join(result.params, {
            name: paramDecl.name,
            type: paramDecl.type.qualType,
            annotations: paramAnnotations
        });
    }

    // doc comment
    const commentText = single(decl.inner, (attr => attr.kind === 'FullComment'));

    if (commentText) {
        const commentData = parseComment(`function:${decl.name}`, location, commentText, options);

        if (result.params) {
            if (options.strict && (!commentData.params || result.params.length > commentData.params.length)) {
                throw new ParserError(`not all params are documented`, location);
            }

            if (options.strict && result.params.length < commentData.params.length) {
                throw new ParserError(`additional params are documented`, location);
            }
        }

        if (commentData.params) {
            for (const i in result.params) {
                let match;

                for (const j in commentData.params) {
                    if (result.params[i].name === commentData.params[j].name) {
                        match = j;
                        break;
                    }
                }

                if (options.strict && (!match || match != i)) {
                    throw new ParserError(
                        `param documentation does not match param name '${result.params[i].name}'`,
                    location);
                }

                if (match) {
                    result.params[i] = { ...result.params[i], ...commentData.params[match] };
                }
            }
        } else if (options.strict && result.params) {
            throw new ParserError(`no params documented for ${decl.name}`, location);
        }

        if (options.strict && !commentData.returns && result.returns.type != 'void') {
            throw new ParserError(`return information is not documented for ${decl.name}`, location);
        }

        result.returns = { ...result.returns, ...commentData.returns };

        delete commentData.params;
        delete commentData.returns;

        result = { ...result, ...commentData };
    }
    else if (options.strict) {
        throw new ParserError(`no documentation for function ${decl.name}`, location);
    }

    return result;
}

function parseEnum(location, decl, options) {
    let result = {
        kind: 'enum',
        id: groupifyId(location, decl.id),
        name: decl.name,
        referenceName: decl.name ? `enum ${decl.name}` : undefined,
        members: [ ],
        comment: undefined,
        location: {...location}
    };

    for (const member of decl.inner.filter(attr => attr.kind === 'EnumConstantDecl')) {
        ensure('enum constant has a name', member.name);

        const explicitValue = single(member.inner, (attr => attr.kind === 'ConstantExpr'));
        const implicitValue = single(member.inner, (attr => attr.kind === 'ImplicitCastExpr'));
        const commentText = single(member.inner, (attr => attr.kind === 'FullComment'));
        const commentData = commentText ? parseComment(`enum:${decl.name}:member:${member.name}`, location, commentText, options) : undefined;

        let value = undefined;

        if (explicitValue && explicitValue.value) {
            value = explicitValue.value;
        } else if (implicitValue) {
            const innerExplicit = single(implicitValue.inner, (attr => attr.kind === 'ConstantExpr'));

            value = innerExplicit?.value;
        }

        result.members.push({
            name: member.name,
            value: value,
            ...commentData
        });
    }

    const commentText = single(decl.inner, (attr => attr.kind === 'FullComment'));

    if (commentText) {
        result = { ...result, ...parseComment(`enum:${decl.name}`, location, commentText, options) };
    }

    return result;
}

function resolveFunctionPointerTypedef(location, typedef) {
    const signature = typedef.type.match(/^((?:const )?[^\s]+(?:\s+\*+)?)\s*\(\*\)\((.*)\)$/);
    const [ , returnType, paramData ] = signature;
    const params = paramData.split(/,\s+/);

    if (options.strict && (!typedef.params || params.length != typedef.params.length)) {
        throw new ParserError(`not all params are documented for function pointer typedef ${typedef.name}`, typedef.location);
    }

    if (!typedef.params) {
        typedef.params = [ ];
    }

    for (const i in params) {
        if (!typedef.params[i]) {
            typedef.params[i] = { };
        }

        typedef.params[i].type = params[i];
    }

    if (typedef.returns === undefined && returnType === 'void') {
        typedef.returns = { type: 'void' };
    }
    else if (typedef.returns !== undefined) {
        typedef.returns.type = returnType;
    }
    else if (options.strict) {
        throw new ParserError(`return type is not documented for function pointer typedef ${typedef.name}`, typedef.location);
    }
}

function parseTypedef(location, decl, options) {
    updateLocation(location, decl);

    let result = {
        kind: 'typedef',
        id: groupifyId(location, decl.id),
        name: ensureDefined('name', decl.name),
        type: ensureDefined('type.qualType', decl.type.qualType),
        targetId: undefined,
        comment: undefined,
        location: {...location}
    };

    const elaborated = single(decl.inner, (attr => attr.kind === 'ElaboratedType'));
    if (elaborated !== undefined && elaborated.ownedTagDecl?.id) {
        result.targetId = groupifyId(location, elaborated.ownedTagDecl?.id);
    }

    const commentText = single(decl.inner, (attr => attr.kind === 'FullComment'));

    if (commentText) {
        const commentData = parseComment(`typedef:${decl.name}`, location, commentText, options);
        result = { ...result, ...commentData };
    }

    if (isFunctionPointer(result.type)) {
        resolveFunctionPointerTypedef(location, result);
    }

    return result;
}

function parseStruct(location, decl, options) {
    let result = {
        kind: 'struct',
        id: groupifyId(location, decl.id),
        name: decl.name,
        referenceName: decl.name ? `struct ${decl.name}` : undefined,
        comment: undefined,
        members: [ ],
        location: {...location}
    };

    for (const member of decl.inner.filter(attr => attr.kind === 'FieldDecl')) {
        let memberData = {
            'name': member.name,
            'type': member.type.qualType
        };

        const commentText = single(member.inner, (attr => attr.kind === 'FullComment'));

        if (commentText) {
            memberData = {...memberData, ...parseComment(`struct:${decl.name}:member:${member.name}`, location, commentText, options)};
        }

        result.members.push(memberData);
    }

    const commentText = single(decl.inner, (attr => attr.kind === 'FullComment'));

    if (commentText) {
        const commentData = parseComment(`struct:${decl.name}`, location, commentText, options);
        result = { ...result, ...commentData };
    }

    return result;
}

function newResults() {
    return {
        all: [ ],
        functions: [ ],
        enums: [ ],
        typedefs: [ ],
        structs: [ ],
        macros: [ ]
    };
};

const returnMap = { };
const paramMap = { };

function simplifyType(givenType) {
    let type = givenType;

    if (type.startsWith('const ')) {
        type = type.substring(6);
    }

    while (type.endsWith('*') && type !== 'void *' && type !== 'char *') {
        type = type.substring(0, type.length - 1).trim();
    }

    if (!type.length) {
        throw new Error(`invalid type: ${result.returns.extendedType || result.returns.type}`);
    }

    return type;
}

function createAndPush(arr, name, value) {
    if (!arr[name]) {
        arr[name] = [ ];
    }

    if (arr[name].length && arr[name][arr[name].length - 1] === value) {
        return;
    }

    arr[name].push(value);
}

function addReturn(result) {
    if (!result.returns) {
        return;
    }

    let type = simplifyType(result.returns.extendedType?.type || result.returns.type);

    createAndPush(returnMap, type, result.name);
}

function addParameters(result) {
    if (!result.params) {
        return;
    }

    for (const param of result.params) {
        let type = param.extendedType?.type || param.type;

        if (!type && options.strict) {
            throw new Error(`parameter ${result.name} erroneously documented when not specified`);
        } else if (!type) {
            continue;
        }

        type = simplifyType(type);

        if (param.direction === 'out') {
            createAndPush(returnMap, type, result.name);
        }
        else {
            createAndPush(paramMap, type, result.name);
        }
    }
}

function addResult(results, result) {
    results[`${result.kind}s`].push(result);
    results.all.push(result);

    addReturn(result);
    addParameters(result);
}

function mergeResults(one, two) {
    const results = newResults();

    for (const inst of Object.keys(results)) {
        results[inst].push(...one[inst]);
        results[inst].push(...two[inst]);
    }

    return results;
}

function getById(results, id) {
    ensure("id is set", id !== undefined);
    return single(results.all.all, (item => item.id === id), id);
}

function getByKindAndName(results, kind, name) {
    ensure("kind is set", kind !== undefined);
    ensure("name is set", name !== undefined);
    return single(results.all[`${kind}s`], (item => item.name === name), name);
}

function getByName(results, name) {
    ensure("name is set", name !== undefined);
    return single(results.all.all, (item => item.name === name), name);
}

function isFunctionPointer(type) {
    return type.match(/^(?:const )?[A-Za-z0-9_]+\s+\**\(\*/);
}

function resolveCallbacks(results) {
    // expand callback types
    for (const fn of results.all.functions) {
        for (const param of fn.params || [ ]) {
            const typedef = getByName(results, param.type);

            if (typedef === undefined) {
                continue;
            }

            param.referenceType = typedef.type;
        }
    }

    for (const struct of results.all.structs) {
        for (const member of struct.members) {
            const typedef = getByKindAndName(results, 'typedef', member.type);

            if (typedef === undefined) {
                continue;
            }

            member.referenceType = typedef.type;
        }
    }
}

function trimBase(path) {
    if (!path) {
        return path;
    }

    for (const segment of [ 'git2', 'git' ]) {
        const base = [ includeBase(path), segment ].join('/');

        if (path.startsWith(base + '/')) {
            return path.substr(base.length + 1);
        }
    }

    throw new Error(`header path ${path} is not beneath standard root`);
}

function resolveTypedefs(results) {
    for (const typedef of results.all.typedefs) {
        let target = typedef.targetId ? getById(results, typedef.targetId) : undefined;

        if (target) {
            // update the target's preferred name with the short name
            target.referenceName = typedef.name;

            if (target.name === undefined) {
                target.name = typedef.name;
            }
        }
        else if (typedef.type.startsWith('struct ')) {
            const path = typedef.location.file;

            /*
             * See if this is actually a typedef to a declared struct,
             * then it is not actually opaque.
             */
            if (results.all.structs.filter(fn => fn.name === typedef.name).length > 0) {
                typedef.opaque = false;
                continue;
            }

            opaque = {
                kind: 'struct',
                id: groupifyId(typedef.location, typedef.id),
                name: typedef.name,
                referenceName: typedef.type,
                opaque: true,
                comment: typedef.comment,
                location: typedef.location,
                group: typedef.group
            };

            addResult(results.files[path], opaque);
            addResult(results.all, opaque);
        }
        else if (isFunctionPointer(typedef.type) ||
                 typedef.type === 'int64_t' ||
                 typedef.type === 'uint64_t') {
            // standard types
            // TODO : make these a list
        }
        else {
            typedef.kind = 'alias';
            typedef.typedef = true;
        }
    }
}

function lastCommentIsGroupDelimiter(decls) {
    if (decls[decls.length - 1].inner &&
        decls[decls.length - 1].inner.length > 0) {
        return lastCommentIsGroupDelimiter(decls[decls.length - 1].inner);
    }

    if (decls.length >= 2 &&
        decls[decls.length - 1].kind.endsWith('Comment') &&
        decls[decls.length - 2].kind.endsWith('Comment') &&
        decls[decls.length - 2].text === '@' &&
        decls[decls.length - 1].text === '{') {
        return true;
    }

    return false;
}

async function parseAst(decls, options) {
    const location = {
        file: undefined,
        line: undefined,
        column: undefined
    };

    const results = newResults();

    /* The first decl might have picked up the javadoc _for the file
     * itself_ based on the file's structure. Remove it.
     */
    if (decls.length && decls[0].inner &&
        decls[0].inner.length > 0 &&
        decls[0].inner[0].kind === 'FullComment' &&
        lastCommentIsGroupDelimiter(decls[0].inner[0].inner)) {
        updateLocation(location, decls[0]);
        delete decls[0].inner[0];
    }

    for (const decl of decls) {
        updateLocation(location, decl);

        ensureDefined('kind', decl.kind);

        if (decl.kind === 'FunctionDecl') {
            addResult(results, await parseFunction({...location}, decl, options));
        }
        else if (decl.kind === 'EnumDecl') {
            addResult(results, parseEnum({...location}, decl, options));
        }
        else if (decl.kind === 'TypedefDecl') {
            addResult(results, parseTypedef({...location}, decl, options));
        }
        else if (decl.kind === 'RecordDecl' && decl.tagUsed === 'struct') {
            if (decl.completeDefinition) {
                addResult(results, parseStruct({...location}, decl, options));
            }
        }
        else if (decl.kind === 'VarDecl') {
            if (options.strict) {
                throw new Error(`unsupported variable declaration ${decl.kind}`);
            }
        }
        else {
            throw new Error(`unknown declaration type ${decl.kind}`);
        }
    }

    return results;
}

function parseCommentForMacro(lines, macroIdx, name) {
    let startIdx = -1, endIdx = 0;
    const commentLines = [ ];

    while (macroIdx > 0 &&
           (line = lines[macroIdx - 1].trim()) &&
           (line.trim() === '' ||
            line.trim().endsWith('\\') ||
            line.trim().match(/^#\s*if\s+/) ||
            line.trim().startsWith('#ifdef ') ||
            line.trim().startsWith('#ifndef ') ||
            line.trim().startsWith('#elif ') ||
            line.trim().startsWith('#else ') ||
            line.trim().match(/^#\s*define\s+${name}\s+/))) {
        macroIdx--;
    }

    if (macroIdx > 0 && lines[macroIdx - 1].trim().endsWith('*/')) {
        endIdx = macroIdx - 1;
    } else {
        return '';
    }

    for (let i = endIdx; i >= 0; i--) {
        if (lines[i].trim().startsWith('/**')) {
            startIdx = i;
            break;
        }
        else if (lines[i].trim().startsWith('/*')) {
            break;
        }
    }

    if (startIdx < 0) {
        return '';
    }

    for (let i = startIdx; i <= endIdx; i++) {
        let line = lines[i].trim();

        if (i == startIdx) {
            line = line.replace(/^\s*\/\*\*\s*/, '');
        }

        if (i === endIdx) {
            line = line.replace(/\s*\*\/\s*$/, '');
        }

        if (i != startIdx) {
            line = line.replace(/^\s*\*\s*/, '');
        }

        if (i == startIdx && (line === '@{' || line.startsWith("@{ "))) {
            return '';
        }

        if (line === '') {
            continue;
        }

        commentLines.push(line);
    }

    return commentLines.join(' ');
}

async function parseInfo(data) {
    const fileHeader = data.match(/(.*)\n+GIT_BEGIN_DECL.*/s);
    const headerLines = fileHeader ? fileHeader[1].split(/\n/) : [ ];

    let lines = [ ];
    const detailsLines = [ ];

    let summary = undefined;
    let endIdx = headerLines.length - 1;

    for (let i = headerLines.length - 1; i >= 0; i--) {
        let line = headerLines[i].trim();

        if (line.match(/^\s*\*\/\s*$/)) {
            endIdx = i;
        }

        if (line.match(/^\/\*\*(\s+.*)?$/)) {
            lines = headerLines.slice(i + 1, endIdx);
            break;
        }
        else if (line.match(/^\/\*(\s+.*)?$/)) {
            break;
        }
    }

    for (let line of lines) {
        line = line.replace(/^\s\*/, '');
        line = line.trim();

        const comment = line.match(/^\@(\w+|{)\s*(.*)/);

        if (comment) {
            if (comment[1] === 'brief') {
                summary = comment[2];
            }
        }
        else if (line != '') {
            detailsLines.push(line);
        }
    }

    const details = detailsLines.length > 0 ? detailsLines.join("\n") : undefined;

    return {
        'summary': summary,
        'details': details
    };
}

async function parseMacros(path, data, options) {
    const results = newResults();
    const lines = data.split(/\r?\n/);

    const macros = { };

    for (let i = 0; i < lines.length; i++) {
        const macro = lines[i].match(/^(\s*#\s*define\s+)([^\s\(]+)(\([^\)]+\))?\s*(.*)/);
        let more = false;

        if (!macro) {
            continue;
        }

        let [ , prefix, name, args, value ] = macro;

        if (name.startsWith('INCLUDE_') || name.startsWith('_INCLUDE_')) {
            continue;
        }

        if (args) {
            name = name + args;
        }

        if (macros[name]) {
            continue;
        }

        macros[name] = true;

        value = value.trim();

        if (value.endsWith('\\')) {
            value = value.substring(0, value.length - 1).trim();
            more = true;
        }

        while (more) {
            more = false;

            let line = lines[++i];

            if (line.endsWith('\\')) {
                line = line.substring(0, line.length - 1);
                more = true;
            }

            value += ' ' + line.trim();
        }

        const comment = parseCommentForMacro(lines, i, name);
        const location = {
            file: path,
            line: i + 1,
            column: prefix.length + 1,
        };

        if (options.strict && !comment) {
            throw new ParserError(`no comment for ${name}`, location);
        }

        addResult(results, {
            kind: 'macro',
            name: name,
            location: location,
            value: value,
            comment: comment,
        });
    }

    return results;
}

function resolveUngroupedTypes(results) {
    const groups = { };

    for (const result of results.all.all) {
        result.group = result.location.file;

        if (result.group.endsWith('.h')) {
            result.group = result.group.substring(0, result.group.length - 2);
            groups[result.group] = true;
        }
    }

    for (const result of results.all.all) {
        if (result.location.file === 'types.h' &&
            result.name.startsWith('git_')) {
            let possibleGroup = result.name.substring(4);

            do {
                if (groupMap[possibleGroup]) {
                    result.group = groupMap[possibleGroup];
                    break;
                }
                else if (groups[possibleGroup]) {
                    result.group = possibleGroup;
                    break;
                }
                else if (groups[`sys/${possibleGroup}`]) {
                    result.group = `sys/${possibleGroup}`;
                    break;
                }

                let match = possibleGroup.match(/^(.*)_[^_]+$/);

                if (!match) {
                    break;
                }

                possibleGroup = match[1];
            } while (true);
        }
    }
}

function resolveReturns(results) {
    for (const result of results.all.all) {
        result.returnedBy = returnMap[result.name];
    }
}

function resolveParameters(results) {
    for (const result of results.all.all) {
        result.parameterTo = paramMap[result.name];
    }
}

async function parseHeaders(sourcePath, options) {
    const results = { all: newResults(), files: { } };

    for (const fullPath of await headerPaths(sourcePath)) {
        const path = trimPath(sourcePath, fullPath);
        const fileContents = await readFile(fullPath);

        const ast = await parseAst(await readAst(fullPath, options), options);
        const macros = await parseMacros(path, fileContents, options);
        const info = await parseInfo(fileContents);

        const filedata = mergeResults(ast, macros);

        filedata['info'] = info;

        results.files[path] = filedata;
        results.all = mergeResults(results.all, filedata);
    }

    resolveCallbacks(results);
    resolveTypedefs(results);

    resolveUngroupedTypes(results);

    resolveReturns(results);
    resolveParameters(results);

    return results;
}

function isFunctionPointer(type) {
    return type.match(/^(const\s+)?[A-Za-z0-9_]+\s+\*?\(\*/);
}
function isEnum(type) {
    return type.match(/^enum\s+/);
}
function isStruct(type) {
    return type.match(/^struct\s+/);
}

/*
 * We keep the `all` arrays around so that we can lookup; drop them
 * for the end result.
 */
function simplify(results) {
    const simplified = {
        'info': { },
        'groups': { }
    };

    results.all.all.sort((a, b) => {
        if (!a.group) {
            throw new Error(`missing group for api ${a.name}`);
        }

        if (!b.group) {
            throw new Error(`missing group for api ${b.name}`);
        }

        const aSystem = a.group.startsWith('sys/');
        const aName = aSystem ? a.group.substr(4) : a.group;

        const bSystem = b.group.startsWith('sys/');
        const bName = bSystem ? b.group.substr(4) : b.group;

        if (aName !== bName) {
            return aName.localeCompare(bName);
        }

        if (aSystem !== bSystem) {
            return aSystem ? 1 : -1;
        }

        if (a.location.file !== b.location.file) {
            return a.location.file.localeCompare(b.location.file);
        }

        if (a.location.line !== b.location.line) {
            return a.location.line - b.location.line;
        }

        return a.location.column - b.location.column;
    });

    for (const api of results.all.all) {
        delete api.id;
        delete api.targetId;

        const type = api.referenceType || api.type;

        if (api.kind === 'typedef' && isFunctionPointer(type)) {
            api.kind = 'callback';
            api.typedef = true;
        }
        else if (api.kind === 'typedef' && (!isEnum(type) && !isStruct(type))) {
            api.kind = 'alias';
            api.typedef = true;
        }
        else if (api.kind === 'typedef') {
            continue;
        }

        if (apiIgnoreList.includes(api.name)) {
            continue;
        }

        // TODO: do a warning where there's a redefinition of a symbol
        // There are occasions where we redefine a symbol. First, our
        // parser is not smart enough to know #ifdef's around #define's.
        // But also we declared `git_email_create_from_diff` twice (in
        // email.h and sys/email.h) for several releases.

        if (!simplified['groups'][api.group]) {
             simplified['groups'][api.group] = { };
             simplified['groups'][api.group].apis = { };
             simplified['groups'][api.group].info = results.files[`${api.group}.h`].info;
        }

        simplified['groups'][api.group].apis[api.name] = api;
    }

    return simplified;
}

function joinArguments(next, previous) {
    if (previous) {
        return [...previous, next];
    }
    return [next];
}

async function findIncludes() {
    const includes = [ ];

    for (const possible of defaultIncludes) {
        const includeFile = `${docsPath}/include/git2/${possible}`;

        try {
            await fs.stat(includeFile);
            includes.push(`git2/${possible}`);
        }
        catch (e) {
            if (e?.code !== 'ENOENT') {
                throw e;
            }
        }
    }

    return includes;
}

async function execGit(path, command) {
    const process = child_process.spawn('git', command, { cwd: path });
    const chunks = [ ];

    return new Promise((resolve, reject) => {
        process.stdout.on('data', (chunk) => {
            chunks.push(chunk);
        });
        process.on('close', (code) => {
            resolve(code == 0 ? Buffer.concat(chunks).toString() : undefined);
        });
        process.on('error', function (err) {
            reject(err);
        });
    });
}

async function readMetadata(path) {
    let commit = await execGit(path, [ 'rev-parse', 'HEAD' ]);

    if (commit) {
        commit = commit.trimEnd();
    }

    let version = await execGit(path, [ 'describe', '--tags', '--exact' ]);

    if (!version) {
        const ref = await execGit(path, [ 'describe', '--all', '--exact' ]);

        if (ref && ref.startsWith('heads/')) {
            version = ref.substr(6);
        }
    }

    if (version) {
        version = version.trimEnd();
    }

    return {
        'version': version,
        'commit': commit
    };
}

program.option('--output <filename>')
       .option('--include <filename>', undefined, joinArguments)
       .option('--no-includes')
       .option('--deprecate-hard')
       .option('--validate-only')
       .option('--strict');
program.parse();

const options = program.opts();

if (program.args.length != 1) {
    console.error(`usage: ${path.basename(process.argv[1])} docs`);
    process.exit(1);
}

const docsPath = program.args[0];

if (options['include'] && !options['includes']) {
    console.error(`usage: cannot combined --include with --no-include`);
    process.exit(1);
}

(async () => {
    try {
        if (options['include']) {
            includes = options['include'];
        }
        else if (!options['includes']) {
            includes = [ ];
        }
        else {
            includes = await findIncludes();
        }

        const parseOptions = {
            deprecateHard: options.deprecateHard || false,
            includeFiles: includes,
            strict: options.strict || false
        };

        const results = await parseHeaders(docsPath, parseOptions);
        const metadata = await readMetadata(docsPath);

        const simplified = simplify(results);
        simplified['info'] = metadata;

	if (!options.validateOnly) {
		console.log(JSON.stringify(simplified, null, 2));
	}
    } catch (e) {
        console.error(e);
        process.exit(1);
    }
})();
