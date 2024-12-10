#!/usr/bin/env node

const markdownit = require('markdown-it');
const { program } = require('commander');

const path = require('node:path');
const fs = require('node:fs/promises');
const process = require('node:process');

const githubPath = 'https://github.com/libgit2/libgit2';

const linkPrefix = '/docs/reference';

const projectTitle = 'libgit2';
const includePath = 'include/git2';

const fileDenylist = [ 'stdint.h' ];
const showVersions = true;

const defaultBranch = 'main';

const markdown = markdownit();
const markdownDefaults = {
    code_inline: markdown.renderer.rules.code_inline
};
markdown.renderer.rules.code_inline = (tokens, idx, options, env, self) => {
    const version = env.__version || defaultBranch;

    const code = tokens[idx].content;
    const text = `<code>${nowrap(sanitize(tokens[idx].content))}</code>`;
    const link = linkForCode(version, code, text);

    return link ? link : text;
};

// globals
const apiData = { };
const versions = [ ];
const versionDeltas = { };

function produceVersionPicker(version, classes, cb) {
    let content = "";

    if (!showVersions) {
        return content;
    }

    content += `          <div class="${classes}">\n`;
    content += `            <span>Version:</span>\n`;
    content += `            <select onChange="window.location.href = this.value">\n`;

    for (const v of versions) {
        const link = cb(v);
        if (link) {
            content += `              <option value="${link}"${v === version ? ' selected' : ''}>${v}</option>\n`;
        }
    }

    content += `            </select>\n`;

    content += `          </div>\n`;

    return content;
}

function produceBreadcrumb(version, api, type) {
    let content = "";
    let group = api.group;
    let sys = false;

    if (group.endsWith('.h')) {
        group = group.substr(0, group.length - 2);
    }

    let groupTitle = group;

    if (groupTitle.startsWith('sys/')) {
        groupTitle = groupTitle.substr(4);
        groupTitle += ' (advanced)';
    }

    content += `        <div class="apiBreadcrumb ${type}Breadcrumb">\n`;
    content += `          <ul>\n`;
    content += `            <li><a href="${linkPrefix}/${version}/index.html">API Documentation</a></li>\n`;
    content += `            <li><a href="${linkPrefix}/${version}/${group}/index.html">${groupTitle}</a></li>\n`;
    content += `          </ul>\n`;
    content += `        </div>\n`;

    return content;
}

function produceHeader(version, api, type) {
    let content = "";

    content += `        <div class="apiHeader ${type}Header">\n`;
    content += `          <h2 class="apiName ${type}Name">${api.name}</h2>\n`;

    content += produceAttributes(version, api, type);
    content += produceSearchArea(version, type);

    content += produceVersionPicker(version,
        `apiHeaderVersionSelect ${type}HeaderVersionSelect`,
        (v) => {
            const versionedApi = selectApi(v, (i => i.name === api.name));
            return versionedApi ? linkFor(v, versionedApi) : undefined;
        });

    content += `        </div>\n`;
    content += `\n`;

    return content;
}

function produceAttributes(version, api, type) {
    let content = "";

    if (api.deprecations) {
        content += `          <span class="apiAttribute ${type}Attribute apiAttributeDeprecated ${type}AttributeDeprecated">Deprecated</span>\n`;
    }

    return content;
}

function produceDescription(version, desc, type) {
    let content = "";

    if (! desc.comment) {
        return content;
    }

    content += `\n`;
    content += `        <div class="apiDescription ${type}Description">\n`;

    for (const para of Array.isArray(desc.comment) ? desc.comment : [ desc.comment ]) {
        content += `        ${markdown.render(para, { __version: version })}\n`;
    }

    content += `        </div>\n`;

    return content;
}

function produceList(version, api, type, listType) {
    let content = "";

    if (!api[listType]) {
        return content;
    }

    const listTypeUpper = listType.charAt(0).toUpperCase() + listType.slice(1);
    const listTypeTitle = listTypeUpper.replaceAll(/(.)([A-Z])/g, (match, one, two) => { return one + ' ' + two; });

    content += `\n`;
    content += `        <h3 class="api${listTypeUpper}Header ${type}${listTypeUpper}Header">${listTypeTitle}</h3>\n`;

    content += `        <div class="api${listTypeUpper} ${type}${listTypeUpper}">\n`;
    content += `          <ul>\n`;

    for (const item of api[listType]) {
        content += `          <li>\n`;
        content += `            ${linkText(version, item)}\n`;
        content += `          </li>\n`;
    }

    content += `          </ul>\n`;
    content += `        </div>\n`;

    return content;
}

function produceNotes(version, api, type) {
    return produceList(version, api, type, 'notes');
}

function produceSeeAlso(version, api, type) {
    return produceList(version, api, type, 'deprecated');
}

function produceSeeAlso(version, api, type) {
    return produceList(version, api, type, 'see');
}

function produceWarnings(version, api, type) {
    return produceList(version, api, type, 'warnings');
}

function produceDeprecations(version, api, type) {
    return produceList(version, api, type, 'deprecations');
}

function produceGitHubLink(version, api, type) {
    if (!api || !api.location || !api.location.file) {
        return undefined;
    }

    let file = api.location.file;

    let link = githubPath + '/blob/' + version + '/' + includePath + '/' + file;

    if (api.location.line) {
        link += '#L' + api.location.line;
    }

    return link;
}

function produceSignatureForFunction(version, api, type) {
    let content = "";
    let paramCount = 0;

    let prefix = type === 'callback' ? 'typedef' : '';
    const returnType = api.returns?.type || 'int';

    const githubLink = produceGitHubLink(version, api, type);

    content += `\n`;

    content += `        <h3 class="apiSignatureHeader ${type}SignatureHeader">Signature</h3>\n`;

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    content += `        <div class="apiSignature ${type}Signature">\n`;

    content += `          ${prefix ? prefix + ' ' : ''}${returnType}`;
    content += returnType.endsWith('*') ? '' : ' ';
    content += `${api.name}(`;

    for (const param of api.params || [ ]) {
        content += (paramCount++ > 0) ? ', ' : '';

        if (!param.type && options.strict) {
            throw new Error(`param ${param.name} has no type for function ${api.name}`);
        }
        else if (!param.type) {
            continue;
        }

        content += `<span class="apiSignatureParameter ${type}SignatureParameter">`;
        content += `${param.type}`;
        content += param.type.endsWith('*') ? '' : ' ';

        if (param.name) {
            content += `${param.name}`;
        }

        content += `</span>`;
    }

    content += `);\n`;
    content += `        </div>\n`;

    return content;
}

function produceFunctionParameters(version, api, type) {
    let content = "";

    if (!api.params || api.params.length == 0) {
        return content;
    }

    content += `\n`;

    content += `        <h3 class="apiParametersHeader ${type}ParametersHeader">Parameters</h3>\n`;
    content += `        <div class="apiParameters ${type}Parameters">\n`;

    for (const param of api.params) {
        let direction = param.direction || 'in';
        direction = direction.charAt(0).toUpperCase() + direction.slice(1);

        if (!param.type && options.strict) {
            throw new Error(`param ${param.name} has no type for function ${api.name}`);
        }
        else if (!param.type) {
            continue;
        }

        content += `          <div class="apiParameter ${type}Parameter apiParameter${direction} ${type}Parameter${direction}">\n`;
        content += `            <div class="apiParameterType ${type}ParameterType">\n`;
        content += `              ${linkType(version, param.type)}\n`;
        content += `            </div>\n`;

        if (param.extendedType) {
            content += `            <div class="apiParameterTypeExtended ${type}ParameterTypeExtended">\n`;
            content += `              ${linkType(version, param.extendedType.type)}\n`;
            content += `            </div>\n`;
        }

        content += `            <div class="apiParameterDirection ${type}ParameterDirection apiParameterDirection${direction} ${type}ParameterDirection${direction}">\n`;

        content += `              ${direction}\n`;
        content += `            </div>\n`;

        if (param.name) {
            content += `            <div class="apiParameterName ${type}ParameterName">\n`;
            content += `              ${param.name}\n`;
            content += `            </div>\n`;
        }

        content += `            <div class="apiParameterDescription ${type}ParameterDescription">\n`;
        content += `              ${render(version, param.comment)}\n`;
        content += `            </div>\n`;
        content += `          </div>\n`;
    }

    content += `        </div>\n`;

    return content;
}

function produceFunctionReturn(version, api, type) {
    let content = "";

    if (api.returns && api.returns.type && api.returns.type !== 'void') {
        content += `\n`;
        content += `        <h3 class="apiReturnHeader ${type}ReturnHeader">Returns</h3>\n`;
        content += `        <div class="apiReturn ${type}Return">\n`;
        content += `          <div class="apiReturnType ${type}ReturnType">\n`;
        content += `            ${linkType(version, api.returns.type)}\n`;
        content += `          </div>\n`;
        content += `          <div class="apiReturnDescription ${type}ReturnDescription">\n`;
        content += `            ${render(version, api.returns.comment)}\n`;
        content += `          </div>\n`;
        content += `        </div>\n`;
    }

    return content;
}

function produceSignatureForObject(version, api, type) {
    let content = "";

    const githubLink = produceGitHubLink(version, api, type);

    content += `\n`;

    content += `        <h3 class="apiSignatureHeader ${type}SignatureHeader">Signature</h3>\n`;

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    content += `        <div class="apiSignature ${type}Signature">\n`;
    content += `          typedef ${api.referenceName} ${api.name}\n`;
    content += `        </div>\n`;

    return content;
}

function produceSignatureForStruct(version, api, type) {
    let content = "";

    const githubLink = produceGitHubLink(version, api, type);

    content += `\n`;

    content += `        <h3 class="apiSignatureHeader ${type}SignatureHeader">Signature</h3>\n`;

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    const typedef = api.name.startsWith('struct') ? '' : 'typedef ';

    content += `        <div class="apiSignature ${type}Signature">\n`;
    content += `          ${typedef}struct ${api.name} {\n`;

    for (const member of api.members || [ ]) {
        content += `<span class="apiSignatureMember ${type}SignatureMember">`;
        content += `${member.type}`;
        content += member.type.endsWith('*') ? '' : ' ';

        if (member.name) {
            content += `${member.name}`;
        }

        content += `</span>\n`;
    }

    content += `          };\n`;
    content += `        </div>\n`;

    return content;
}

function isOctalEnum(version, api, type) {
    return api.name === 'git_filemode_t';
}

function isFlagsEnum(version, api, type) {
    // TODO: also handle the flags metadata instead of always just guessing
    if (type !== 'enum') {
        return false;
    }

    let largest = 0;

    for (const member of api.members) {
        if (member.value === undefined) {
            return false;
        }

        if (member.value && (member.value & (member.value - 1))) {
            return false;
        }

        largest = member.value;
    }

    return (largest > 1);
}

function flagsOctal(v) {
    const n = parseInt(v);
    return n ? `0${n.toString(8)}` : 0;
}

function flagsValue(v) {
    if (v === '0') {
        return '0';
    }

    return `(1 << ${Math.log2(v)})`;
}

function produceMembers(version, api, type) {
    let content = "";
    let value = 0;

    if (!api.members || api.members.length == 0) {
        return "";
    }

    let title = type === 'enum' ? 'Values' : 'Members';
    const isOctal = isOctalEnum(version, api, type);
    const isFlags = isFlagsEnum(version, api, type);

    content += `\n`;

    content += `        <h3 class="apiMembersHeader ${type}MembersHeader">${title}</h3>\n`;

    const githubLink = api.kind === 'struct' ? undefined : produceGitHubLink(version, api, type);

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    content += `        <div class="apiMembers ${type}Members">\n`;

    for (const member of api.members) {
        value = member.value ? member.value : value;

        content += `          <div class="apiMember ${type}Member">\n`;

        if (type === 'struct') {
            content += `            <div class="apiMemberType ${type}MemberType">\n`;
            content += `              ${linkType(version, member.type)}\n`;
            content += `            </div>\n`;
        }

        content += `            <div class="apiMemberName ${type}MemberName">\n`;
        content += `              ${member.name}\n`;
        content += `            </div>\n`;

        if (type === 'enum') {
            const enumValue = isOctal ? flagsOctal(value) : (isFlags ? flagsValue(value) : value);

            content += `            <div class="apiMemberValue ${type}MemberValue">\n`;
            content += `              ${enumValue}\n`;
            content += `            </div>\n`;
        }

        content += `            <div class="apiMemberDescription ${type}MemberDescription">\n`;
        content += `              ${render(version, member.comment)}\n`;
        content += `            </div>\n`;
        content += `          </div>\n`;

        value++;
    }

    content += `        </div>\n`;

    return content;
}

function produceReturnedBy(version, api, type) {
    return produceList(version, api, type, 'returnedBy');
}

function produceParameterTo(version, api, type) {
    return produceList(version, api, type, 'parameterTo');
}

function produceVersionDeltas(version, api, type) {
    let content = '';

    if (!showVersions) {
        return content;
    }

    const deltas = versionDeltas[api.name];
    if (!deltas) {
        throw new Error(`no version information for ${api.kind} ${api.name}`);
    }

    content += `        <h3 class="apiVersionsHeader ${type}VersionsHeader">Versions</h3>\n`;
    content += `        <div class="apiVersions ${type}Versions">\n`;
    content += `          <ul>\n`;

    for (const idx in deltas) {
        const item = deltas[idx];

        if (idx == deltas.length - 1) {
            content += `          <li class="apiVersionDeltaIntroduced ${type}VersionDeltaIntroduced">\n`;
        } else if (item.changed) {
            content += `          <li class="apiVersionDeltaChanged ${type}VersionDeltaChanged">\n`;
        } else {
            content += `          <li class="apiVersionDeltaUnchanged ${type}VersionDeltaUnchanged">\n`;
        }

        content += `            <a href="${linkFor(item.version, item.api)}" class="apiVersionLink ${item.api.kind}VersionLink">${item.version}</a>\n`;
        content += `          </li>\n`;
    }

    content += `          </ul>\n`;
    content += `        </div>\n`;

    return content;
}

async function layout(data) {
    let layout;

    if (options.layout) {
        layout = await fs.readFile(options.layout);
    }
    else if (options.jekyllLayout) {
        layout = `---\ntitle: {{title}}\nlayout: ${options.jekyllLayout}\n---\n\n{{content}}`;
    }
    else {
        return data.content;
    }

    return layout.toString().replaceAll(/{{([a-z]+)}}/g, (match, p1) => data[p1] || "");
}

function produceSearchArea(version, type) {
    let content = "";

    content += `\n`;
    content += `           <script src="/js/minisearch.js"></script>\n`;
    content += `           <script src="/js/search.js"></script>\n`;

    content += `           <div class="headerSearchArea ${type}HeaderSearchArea" id="headersearcharea">\n`;
    content += `             <input class="headerSearchBox ${type}HeaderSearchBox" id="headersearchbox" placeholder="Search..." onInput="handleSearchSuggest({ version: '${version}' })" onFocusIn="handleSearchSuggest({ version: '${version}' })" onKeyUp="if (event.code === 'Enter') { submitSearch({ version: '${version}' }); }"/>\n`;
    content += `             <div class="headerSearchResults ${type}HeaderSearchResults" id="headersearchresults">\n`;
    content += `             </div>\n`;
    content += `           </div>\n`;
    content += `\n`;

    return content;
}

async function produceDocumentationForApi(version, api, type) {
    let content = "";

    content += `      <div class="api ${type}">\n`;

    content += produceBreadcrumb(version, api, type);
    content += produceHeader(version, api, type);
    content += produceDescription(version, api, type);
    content += produceNotes(version, api, type);
    content += produceDeprecations(version, api, type);
    content += produceSeeAlso(version, api, type);
    content += produceWarnings(version, api, type);
    content += produceSignature(version, api, type);
    content += produceMembers(version, api, type);
    content += produceFunctionParameters(version, api, type);
    content += produceFunctionReturn(version, api, type);
    content += produceReturnedBy(version, api, type);
    content += produceParameterTo(version, api, type);
    content += produceVersionDeltas(version, api, type);

    content += `      </div>\n`;


    const name = (type === 'macro' && api.name.includes('(')) ?
        api.name.replace(/\(.*/, '') : api.name;

    const groupDir = `${outputPath}/${version}/${api.group}`;
    const filename = `${groupDir}/${name}.html`;

    await fs.mkdir(groupDir, { recursive: true });
    await fs.writeFile(filename, await layout({
        title: `${api.name} (${projectTitle} ${version})`,
        content: content
    }));
}

function selectApi(version, cb) {
    const allApis = allApisForVersion(version, apiData[version]['groups']);

    for (const name in allApis) {
        const api = allApis[name];

        if (cb(api)) {
            return api;
        }
    }

    return undefined;
}

function apiFor(version, type) {
    return selectApi(version, ((api) => api.name === type));
}

function linkFor(version, api) {
    const name = (api.kind === 'macro' && api.name.includes('(')) ?
        api.name.replace(/\(.*/, '') : api.name;

    return `${linkPrefix}/${version}/${api.group}/${name}.html`;
}

function linkForCode(version, code, text) {
    let api = selectApi(version, ((api) => api.name === code));
    let valueDecl = undefined;

    const apisForVersion = allApisForVersion(version, apiData[version]['groups']);

    if (!api) {
        for (const enumDecl of Object.values(apisForVersion).filter(api => api.kind === 'enum')) {
            const member = enumDecl.members.filter((m) => m.name === code);

            if (member && member[0]) {
                api = enumDecl;
                valueDecl = member[0];
                break;
            }
        }
    }

    if (!api) {
        return undefined;
    }

    const kind = internalKind(version, api);
    let link = linkFor(version, api);

    if (valueDecl) {
        link += `#${valueDecl.name}`;
    }

    if (!text) {
        text = `<code>${sanitize(code)}</code>`;
    }

    return `<a href="${link}" class="apiLink ${kind}Link">${text}</a>`;
}

function linkType(version, given) {
    let type = given;

    if ((content = given.match(/^(?:const\s+)?([A-Za-z0-9_]+)(?:\s+\*+)?/))) {
        type = content[1];
    }

    const api = apiFor(version, type);

    if (api) {
        return `<a href="${linkFor(version, api)}" class="apiLink ${internalKind(version, api)}Link">${given}</a>`;
    }

    return given;
}

function linkText(version, str) {
    const api = apiFor(version, str);

    if (api) {
        return `<a href="${linkFor(version, api)}" className="apiLink ${internalKind(version, api)}Link">${str}</a>`;
    }

    return sanitize(str);
}

function render(version, str) {
    let content = [ ];

    if (!str) {
        return '';
    }

    for (const s of Array.isArray(str) ? str : [ str ] ) {
        content.push(markdown.render(s, { __version: version }).replaceAll(/\s+/g, ' '));
    }

    return content.join(' ');
}

function nowrap(text) {
    text = text.replaceAll(' ', '</span> <span class="codeWord">');
    text = `<span class="codeWord">${text}</span>`;
    return text;
}

function sanitize(str) {
    let content = [ ];

    if (!str) {
        return '';
    }

    for (const s of Array.isArray(str) ? str : [ str ] ) {
        content.push(s.replaceAll('&', '&amp;')
                     .replaceAll('<', '&lt;')
                     .replaceAll('>', '&gt;')
                     .replaceAll('{', '&#123;')
                     .replaceAll('}', '&#125;'));
    }

    return content.join(' ');
}

function produceSignatureForAlias(version, api, type) {
    let content = "";

    const githubLink = produceGitHubLink(version, api, type);

    content += `        <h3 class="apiSignatureHeader ${type}SignatureHeader">Signature</h3>\n`;

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    content += `        <div class="apiSignature ${type}Signature">\n`;
    content += `          typedef ${api.name} ${api.type};`;
    content += `        </div>\n`;

    return content;
}

function produceSignatureForMacro(version, api, type) {
    let content = "";

    const githubLink = produceGitHubLink(version, api, type);

    content += `        <h3 class="apiSignatureHeader ${type}SignatureHeader">Signature</h3>\n`;

    if (githubLink) {
        content += `        <div class="apiSignatureLink ${type}SignatureLink">\n`;
        content += `          <a href="${githubLink}" target="_blank" rel="noopener">GitHub</a>\n`;
        content += `        </div>\n`;
    }

    content += `        <div class="apiSignature ${type}Signature">\n`;
    content += `          #define ${api.name} ${sanitize(api.value)}`;
    content += `        </div>\n`;

    return content;
}

function produceSignature(version, api, type) {
    if (type === 'macro') {
        return produceSignatureForMacro(version, api, type);
    }
    else if (type === 'alias') {
        return produceSignatureForAlias(version, api, type);
    }
    else if (type === 'function' || type === 'callback') {
        return produceSignatureForFunction(version, api, type);
    }
    else if (type === 'object') {
        return produceSignatureForObject(version, api, type);
    }
    else if (type === 'struct') {
        return produceSignatureForStruct(version, api, type);
    }
    else if (type === 'struct' || type === 'enum') {
        return "";
    }
    else {
        throw new Error(`unknown type: ${api.kind}`);
    }
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

function internalKind(version, api) {
    if (api.kind === 'struct' && api.opaque) {
        return 'object';
    }

    return api.kind;
}

function externalKind(kind) {
    if (kind === 'object') {
        return 'struct';
    }

    return kind;
}

async function produceIndexForGroup(version, group, versionApis) {
    let content = "";

    if (versionApis['groups'][group].apis.length == 0) {
        return;
    }

    const apis = Object.values(versionApis['groups'][group].apis);

    let fileName = group;
    if (fileName.endsWith('.h')) {
        fileName = fileName.substr(0, fileName.length - 2);
    }

    const system = fileName.startsWith('sys/');
    let groupName = system ? fileName.substr(4) : fileName;

    content += `    <div class="group">\n`;

    content += `      <div class="groupBreadcrumb">\n`;
    content += `        <ul>\n`;
    content += `          <li><a href="${linkPrefix}/${version}/index.html">API Documentation</a></li>\n`;
    content += `        </ul>\n`;
    content += `      </div>\n`;

    content += `      <div class="groupHeader">\n`;
    content += `        <h2 class="groupName">${groupName}</h2>\n`;

    content += produceSearchArea(version, 'group');
    content += produceVersionPicker(version, "groupHeaderVersionSelect", (v) => {
        if (apiData[v]['groups'][group]) {
            return `${linkPrefix}/${v}/${groupName}/index.html`;
        }
        return undefined;
    });

    content += `      </div>\n`;

    let details = undefined;

    if (versionApis['groups'][group].info?.details) {
        details = markdown.render(versionApis['groups'][group].info.details, { __version: version });
    } else if (versionApis['groups'][group].info?.summary) {
        details = versionApis['groups'][group].info.summary;
    }

    if (details) {
        content += `      <div class="groupIndexDetails">\n`;
        content += `        ${details}\n`;
        content += `      </div>\n`;
    }

    for (const kind of [ 'object', 'struct', 'macro', 'enum', 'callback', 'alias', 'function' ]) {
        content += produceIndexForApiKind(version, apis.filter(api => {
            if (kind === 'object') {
                return api.kind === 'struct' && api.opaque;
            }
            else if (kind === 'struct') {
                return api.kind === 'struct' && !api.opaque;
            }
            else {
                return api.kind === kind;
            }
        }), kind);
    }

    content += `    </div>\n`;

    const groupsDir = `${outputPath}/${version}/${fileName}`;
    const filename = `${groupsDir}/index.html`;

    await fs.mkdir(groupsDir, { recursive: true });
    await fs.writeFile(filename, await layout({
        title: `${groupName} APIs (${projectTitle} ${version})`,
        content: content
    }));
}

async function produceDocumentationForApis(version, apiData) {
    const apis = allApisForVersion(version, apiData['groups']);

    for (const func of Object.values(apis).filter(api => api.kind === 'function')) {
        await produceDocumentationForApi(version, func, 'function');
    }

    for (const struct of Object.values(apis).filter(api => api.kind === 'struct')) {
        await produceDocumentationForApi(version, struct, internalKind(version, struct));
    }

    for (const e of Object.values(apis).filter(api => api.kind === 'enum')) {
        await produceDocumentationForApi(version, e, 'enum');
    }

    for (const callback of Object.values(apis).filter(api => api.kind === 'callback')) {
        await produceDocumentationForApi(version, callback, 'callback');
    }

    for (const alias of Object.values(apis).filter(api => api.kind === 'alias')) {
        await produceDocumentationForApi(version, alias, 'alias');
    }

    for (const macro of Object.values(apis).filter(api => api.kind === 'macro')) {
        await produceDocumentationForApi(version, macro, 'macro');
    }
}

function produceIndexForApiKind(version, apis, kind) {
    let content = "";

    if (!apis || !apis.length) {
        return content;
    }

    let kindUpper = kind.charAt(0).toUpperCase() + kind.slice(1);
    kindUpper += (kind === 'alias') ? 'es' : 's';

    content += `\n`;
    content += `        <h3 class="apiListHeader ${kind}ListHeader">${kindUpper}</h3>\n`;

    content += `        <div class="apiList ${kind}List">\n`;

    for (const item of apis) {
        if (item.changed) {
            content += `          <div class="apiListItemChanged ${kind}ListItemChanged">\n`;
        } else {
            content += `          <div class="apiListItem ${kind}ListItem">\n`;
        }

        content += `            <div class="apiListName ${kind}ListName">\n`;
        content += `              <a href="${linkFor(version, item)}">\n`;
        content += `                ${item.name}\n`;
        content += `              </a>\n`;
        content += `            </div>\n`;

        let shortComment = Array.isArray(item.comment) ? item.comment[0] : item.comment;
        shortComment = shortComment || '';

        shortComment = shortComment.replace(/\..*/, '');

        content += `            <div class="apiListItemDescription ${kind}ListItemDescription">\n`;
        content += `              ${render(version, shortComment)}\n`;
        content += `            </div>\n`;
        content += `          </div>\n`;
    }

    content += `        </div>\n`;

    return content;
}

function versionIndexContent(version, apiData) {
    let content = "";
    let hasSystem = false;

    content += `      <div class="version">\n`;
    content += `        <div class="versionHeader">\n`;
    content += `          <h2 class="versionName">${projectTitle} ${version}</h2>\n`;

    content += produceSearchArea(version, 'version');
    content += produceVersionPicker(version, "versionHeaderVersionSelect",
        (v) => `${linkPrefix}/${v}/index.html`);

    content += `        </div>\n`;

    content += `\n`;
    content += `        <h3>Groups</h3>\n`;
    content += `        <ul class="versionIndexStandard">\n`;

    for (const group of Object.keys(apiData['groups']).sort((a, b) => {
        if (a.startsWith('sys/')) { return 1; }
        if (b.startsWith('sys/')) { return -1; }
        return a.localeCompare(b);
    }).map(fn => {
        let n = fn;
        let sys = false;

        if (n.endsWith('.h')) {
            n = n.substr(0, n.length - 2);
        }

        if (n.startsWith('sys/')) {
            n = n.substr(4);
            sys = true;
        }

        return {
            name: n, filename: fn, system: sys, info: apiData['groups'][fn].info, apis: apiData['groups'][fn]
        };
    }).filter(filedata => {
        return Object.keys(filedata.apis).length > 0 && !fileDenylist.includes(filedata.filename);
    })) {
        if (group.system && !hasSystem) {
            hasSystem = true;

            content += `        </ul>\n`;
            content += `\n`;
            content += `        <h3>System Groups (Advanced)</h3>\n`;
            content += `        <ul class="versionIndexSystem">\n`;
        }

        let link = `${linkPrefix}/${version}/`;
        link += group.system ? `sys/` : '';
        link += group.name;
        link += `/index.html`;

        content += `          <li>\n`;
        content += `            <div class="versionIndexName">\n`;
        content += `              <a href="${link}">\n`;
        content += `                ${group.name}\n`;
        content += `              </a>\n`;
        content += `            </div>\n`;

        if (group.info?.summary) {
            content += `            <div class="versionIndexSummary">\n`;
            content += `              ${group.info.summary}`;
            content += `            </div>\n`;
        }

        content += `          </li>\n`;
    }

    content += `        </ul>\n`;

    content += `      </div>\n`;

    return content;
}

async function produceDocumentationIndex(version, apiData) {
    const content = versionIndexContent(version, apiData);

    const versionDir = `${outputPath}/${version}`;
    const filename = `${versionDir}/index.html`;

    await fs.mkdir(versionDir, { recursive: true });
    await fs.writeFile(filename, await layout({
        title: `APIs (${projectTitle} ${version})`,
        content: content
    }));
}

async function documentationIsUpToDateForVersion(version, apiData) {
    try {
        const existingMetadata = JSON.parse(await fs.readFile(`${outputPath}/${version}/.metadata`));
        return existingMetadata?.commit === apiData.info.commit;
    }
    catch (e) {
    }

    return false;
}

async function produceDocumentationMetadata(version, apiData) {
    const versionDir = `${outputPath}/${version}`;
    const filename = `${versionDir}/.metadata`;

    await fs.mkdir(versionDir, { recursive: true });
    await fs.writeFile(filename, JSON.stringify(apiData.info, null, 2) + "\n");
}

async function cleanupOldDocumentation(version) {
    const versionDir = `${outputPath}/${version}`;

    for (const fn of await fs.readdir(versionDir)) {
        if (fn === '.metadata') {
            continue;
        }

        const path = `${versionDir}/${fn}`;
        await fs.rm(path, { recursive: true });
    }
}

async function produceDocumentationForVersion(version, apiData) {
    if (!options.force && await documentationIsUpToDateForVersion(version, apiData)) {
        if (options.verbose) {
            console.log(`Documentation exists for ${version} at version ${apiData.info.commit.substr(0, 7)}; skipping...`);
        }

        return;
    }

    if (options.verbose) {
        console.log(`Producing documentation for ${version}...`);
    }

    await cleanupOldDocumentation(version);

    await produceDocumentationForApis(version, apiData);

    for (const group in apiData['groups']) {
        await produceIndexForGroup(version, group, apiData);
    }

    await produceDocumentationIndex(version, apiData);

    await produceDocumentationMetadata(version, apiData);
}

function versionDeltaData(version, api) {
    const base = { version: version, api: api };

    if (api.kind === 'function') {
        return {
            ...base,
            returns: api.returns?.type || 'int',
            params: api.params?.map((p) => p.type) || [ 'void' ]
        };
    }
    else if (api.kind === 'enum') {
        return {
            ...base,
            members: api.members?.map((m) => { return { 'name': m.name, 'value': m.value } })
        };
    }
    else if (api.kind === 'callback') {
        return { ...base, };
    }
    else if (api.kind === 'alias') {
        return { ...base, };
    }
    else if (api.kind === 'struct') {
        return {
            ...base,
            members: api.members?.map((m) => { return { 'name': m.name, 'type': m.type } })
        };
    }
    else if (api.kind === 'macro') {
        return {
            ...base,
            name: api.name,
            value: api.value
        };
    }
    else {
        throw new Error(`unknown api kind: '${api.kind}'`);
    }
}

function deltasEqual(a, b) {
    const unversionedA = { ...a };
    const unversionedB = { ...b };

    delete unversionedA.version;
    delete unversionedA.api;
    delete unversionedA.changed;
    delete unversionedB.version;
    delete unversionedB.api;
    delete unversionedB.changed;

    return JSON.stringify(unversionedA) === JSON.stringify(unversionedB);
}

const apiForVersionCache = { };
function allApisForVersion(version, apiData) {
    if (apiForVersionCache[version]) {
        return apiForVersionCache[version];
    }

    let result = { };
    for (const file in apiData['groups']) {
        result = { ...result, ...apiData['groups'][file].apis };
    }

    apiForVersionCache[version] = result;
    return result;
}

function seedVersionApis(apiData) {
    for (const version in apiData) {
        allApisForVersion(version, apiData[version]);
    }
}

function calculateVersionDeltas(apiData) {
    for (const version in apiData) {
        const apisForVersion = allApisForVersion(version, apiData[version]);

        for (const api in apisForVersion) {
            if (!versionDeltas[api]) {
                versionDeltas[api] = [ ];
            }

            versionDeltas[api].push(versionDeltaData(version, apisForVersion[api]));
        }
    }

    for (const api in versionDeltas) {
        const count = versionDeltas[api].length;

        versionDeltas[api][count - 1].changed = true;

        for (let i = count - 2; i >= 0; i--) {
            versionDeltas[api][i].changed = !deltasEqual(versionDeltas[api][i], versionDeltas[api][i + 1]);
        }
    }
}

async function produceSearch(versions) {
    if (options.verbose) {
        console.log(`Producing search page...`);
    }

    let content = "";

    content += `<script src="/js/minisearch.js"></script>\n`;
    content += `<script src="/js/search.js"></script>\n`;
    content += `<script src="/js/markdown-it.js"></script>\n`;
    content += `<script>\n`;
    content += `  const markdown = window.markdownit();\n`;
    content += `</script>\n`;

    content += `\n`;

    content += `      <div class="search">\n`;
    content += `        <div class="searchHeader">\n`;
    content += `          <h2 class="searchName">libgit2 search</h2>\n`;
    content += `          <div class="searchHeaderVersionSelect">\n`;
    content += `            <span>Version:</span>\n`;
    content += `            <select id="searchversion" onChange="setSearchVersion(this.value)">\n`;

    for (const version of versions) {
        content += `              <option value="${version}">${version}</option>\n`;
    }

    content += `            </select>\n`;
    content += `          </div>\n`;
    content += `        </div>\n`;

    content += `\n`;

    content += `        <div class="searchSearchBox">\n`;
    content += `          <input type="text" id="bodysearchbox" placeholder="Search..." onKeyDown="if (event.key === 'Enter') { resetSearch(); }" />\n`;
    content += `          <button onClick="resetSearch()">Search</button>\n`;
    content += `        </div>\n`;

    content += `\n`;

    content += `        <div class="searchResultsArea" id="bodyresultsarea" style="visibility: hidden;">\n`;
    content += `          <h3>Results</h3>\n`;
    content += `          <div class="searchResults" id="bodysearchresults">\n`;
    content += `          </div>\n`;
    content += `        </div>\n`;
    content += `      </div>\n`;

    const filename = `${outputPath}/search.html`;

    await fs.mkdir(outputPath, { recursive: true });
    await fs.writeFile(filename, await layout({
        title: `API search (${projectTitle})`,
        content: content
    }));
}

async function produceMainIndex(versions) {
    const versionDefault = versions[0];

    if (options.verbose) {
        console.log(`Producing documentation index...`);
    }

    let content = "";

    content += `<script>\n`;
    content += `let newLocation = window.location.href;\n`;
    content += `\n`;
    content += `if (newLocation.endsWith('index.html')) {\n`;
    content += `   newLocation = newLocation.substr(0, newLocation.length - 10);\n`;
    content += `}\n`;
    content += `\n`;
    content += `if (!newLocation.endsWith('/')) {\n`;
    content += `  newLocation = newLocation + '/';\n`;
    content += `}\n`;
    content += `\n`;
    content += `newLocation = newLocation + '${versionDefault}/';\n`;
    content += `history.replaceState({}, "APIs (${projectTitle} ${versionDefault})", newLocation);\n`;
    content += `</script>\n`;
    content += `\n`;

    content += versionIndexContent(versionDefault, apiData[versionDefault]);

    const filename = `${outputPath}/index.html`;

    await fs.mkdir(outputPath, { recursive: true });
    await fs.writeFile(filename, await layout({
        title: `APIs (${projectTitle} ${versionDefault})`,
        content: content
    }));
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

program.option('--output <filename>')
       .option('--layout <filename>')
       .option('--jekyll-layout <name>')
       .option('--version <version...>')
       .option('--verbose')
       .option('--force')
       .option('--strict');
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

        versions.push(...v.sort(versionSort).reverse());

        for (const version of versions) {
            if (options.verbose) {
                console.log(`Reading documentation data for ${version}...`);
            }

            apiData[version] = JSON.parse(await fs.readFile(`${docsPath}/${version}.json`));
        }

        if (showVersions) {
            if (options.verbose) {
                console.log(`Calculating version deltas...`);
            }

            calculateVersionDeltas(apiData);
        }

        for (const version of versions) {
            await produceDocumentationForVersion(version, apiData[version]);
        }

        await produceSearch(versions);
        await produceMainIndex(versions);
    } catch (e) {
        console.error(e);
        process.exit(1);
    }
})();
