#!/usr/bin/env python
#
# Copyright (c) Vicent Marti. All rights reserved.
#
# This file is part of clar, distributed under the ISC license.
# For full terms see the included COPYING file.
#

from __future__ import with_statement
from string import Template
import re, fnmatch, os, sys, codecs, pickle

class Module(object):
    class Template(object):
        def __init__(self, module):
            self.module = module

        def _render_callback(self, cb):
            if not cb:
                return '    { NULL, NULL, 0, NULL }'

            return '    { "%s", %s, %d, &%s }' % \
                (cb['short_name'], \
                 '"' + cb['description'] + '"' if cb['description'] != None else "NULL", \
                 cb['runs'], \
                 cb['symbol'])

    class DeclarationTemplate(Template):
        def render(self):
            out = "\n".join("extern %s;" % cb['declaration'] for cb in self.module.callbacks) + "\n"

            for initializer in self.module.initializers:
                out += "extern %s;\n" % initializer['declaration']

            if self.module.reset:
                out += "extern %s;\n" % self.module.reset['declaration']

            if self.module.cleanup:
                out += "extern %s;\n" % self.module.cleanup['declaration']

            return out

    class CallbacksTemplate(Template):
        def render(self):
            out = "static const struct %s_func _%s_cb_%s[] = {\n" % (self.module.app_name, self.module.app_name, self.module.name)
            out += ",\n".join(self._render_callback(cb) for cb in self.module.callbacks)
            out += "\n};\n"
            return out

    class InfoTemplate(Template):
        def render(self):
            templates = []

            initializers = self.module.initializers
            if len(initializers) == 0:
                initializers = [ None ]

            for initializer in initializers:
                name = self.module.clean_name()
                if initializer and initializer['short_name'].startswith('initialize_'):
                    variant = initializer['short_name'][len('initialize_'):]
                    name += " (%s)" % variant.replace('_', ' ')

                template = Template(
            r"""
    {
        "${clean_name}",
    ${initialize},
    ${reset},
    ${cleanup},
        ${cb_ptr}, ${cb_count}, ${enabled}
    }"""
                ).substitute(
                    clean_name = name,
                    initialize = self._render_callback(initializer),
                    reset = self._render_callback(self.module.reset),
                    cleanup = self._render_callback(self.module.cleanup),
                    cb_ptr = "_%s_cb_%s" % (self.module.app_name, self.module.name),
                    cb_count = len(self.module.callbacks),
                    enabled = int(self.module.enabled)
                )
                templates.append(template)

            return ','.join(templates)

    def __init__(self, name, app_name, prefix):
        self.name = name
        self.app_name = app_name
        self.prefix = prefix

        self.mtime = None
        self.enabled = True
        self.modified = False

    def clean_name(self):
        return self.name.replace("_", "::")

    def _skip_comments(self, text):
        SKIP_COMMENTS_REGEX = re.compile(
            r'//.*?$|/\*(?!\s*\[clar\]:).*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
            re.DOTALL | re.MULTILINE)

        def _replacer(match):
            s = match.group(0)
            return "" if s.startswith('/') else s

        return re.sub(SKIP_COMMENTS_REGEX, _replacer, text)

    def parse(self, contents):
        TEST_FUNC_REGEX = r"^(void\s+(%s_%s__(\w+))\s*\(\s*void\s*\))(?:\s*/\*\s*\[clar\]:\s*(.*?)\s*\*/)?\s*\{"

        contents = self._skip_comments(contents)
        regex = re.compile(TEST_FUNC_REGEX % (self.prefix, self.name), re.MULTILINE)

        self.callbacks = []
        self.initializers = []
        self.reset = None
        self.cleanup = None

        for (declaration, symbol, short_name, options) in regex.findall(contents):
            runs = 0
            description = None

            while options != '':
                match = re.search(r'^([a-zA-Z0-9]+)=(\"[^"]*\"|[a-zA-Z0-9_\-]+|\d+)(?:,\s*|\Z)(.*)', options)

                if match == None:
                    print("Invalid options: '%s' for '%s'" % (options, symbol))
                    sys.exit(1)

                key = match.group(1)
                value = match.group(2)
                options = match.group(3)

                match = re.search(r'^\"(.*)\"$', value)
                if match != None:
                    value = match.group(1)

                match = re.search(r'([^a-zA-Z0-9 _\-,\.])', value)
                if match != None:
                    print("Invalid character '%s' in %s for '%s'" % (match.group(1), key, symbol))
                    sys.exit(1)

                if key == "description":
                    description = value
                elif key == "runs":
                    if not value.isnumeric():
                        print("Invalid option: '%s' in runs for '%s'" % (option, symbol))
                        sys.exit(1)
                    runs = int(value)
                else:
                    print("Invalid option: '%s' for '%s'" % (key, symbol))
                    sys.exit(1)

            data = {
                "short_name" : short_name,
                "declaration" : declaration,
                "symbol" : symbol,
                "description" : description,
                "runs" : runs
            }

            if short_name.startswith('initialize'):
                self.initializers.append(data)
            elif short_name == 'reset':
                self.reset = data
            elif short_name == 'cleanup':
                self.cleanup = data
            else:
                self.callbacks.append(data)

        return self.callbacks != []

    def refresh(self, path):
        self.modified = False

        try:
            st = os.stat(path)

            # Not modified
            if st.st_mtime == self.mtime:
                return True

            self.modified = True
            self.mtime = st.st_mtime

            with codecs.open(path, encoding='utf-8') as fp:
                raw_content = fp.read()

        except IOError:
            return False

        return self.parse(raw_content)

class TestSuite(object):

    def __init__(self, path, output):
        self.path = path
        self.output = output

    def should_generate(self, path):
        if not os.path.isfile(path):
            return True

        if any(module.modified for module in self.modules.values()):
            return True

        return False

    def find_modules(self):
        modules = []
        for root, _, files in os.walk(self.path):
            module_root = root[len(self.path):]
            module_root = [c for c in module_root.split(os.sep) if c]

            tests_in_module = fnmatch.filter(files, "*.c")

            for test_file in tests_in_module:
                full_path = os.path.join(root, test_file)
                module_name = "_".join(module_root + [test_file[:-2]]).replace("-", "_")

                modules.append((full_path, module_name))

        return modules

    def load_cache(self, app_name):
        path = os.path.join(self.output, ".%scache" % app_name)
        cache = {}

        try:
            fp = open(path, 'rb')
            cache = pickle.load(fp)
            fp.close()
        except (IOError, ValueError):
            pass

        return cache

    def save_cache(self, app_name):
        path = os.path.join(self.output, ".%scache" % app_name)
        with open(path, 'wb') as cache:
            pickle.dump(self.modules, cache)

    def load(self, app_name, prefix, force = False):
        module_data = self.find_modules()
        self.modules = {} if force else self.load_cache(app_name)

        for path, name in module_data:
            if name not in self.modules:
                self.modules[name] = Module(name, app_name, prefix)

            if not self.modules[name].refresh(path):
                del self.modules[name]

    def disable(self, excluded):
        for exclude in excluded:
            for module in self.modules.values():
                name = module.clean_name()
                if name.startswith(exclude):
                    module.enabled = False
                    module.modified = True

    def suite_count(self):
        return sum(max(1, len(m.initializers)) for m in self.modules.values())

    def callback_count(self):
        return sum(len(module.callbacks) for module in self.modules.values())

    def write(self, name):
        output = os.path.join(self.output, "%s.suite" % name)

        if not self.should_generate(output):
            return False

        with open(output, 'w') as data:
            modules = sorted(self.modules.values(), key=lambda module: module.name)

            for module in modules:
                t = Module.DeclarationTemplate(module)
                data.write(t.render())

            for module in modules:
                t = Module.CallbacksTemplate(module)
                data.write(t.render())

            suites = "static struct %s_suite _%s_suites[] = {" % (name, name)
            suites += ','.join(
                Module.InfoTemplate(module).render() for module in modules
            ) + "\n};\n"

            data.write(suites)

            data.write("static const size_t _%s_suite_count = %d;\n" % (name, self.suite_count()))
            data.write("static const size_t _%s_callback_count = %d;\n" % (name, self.callback_count()))

        self.save_cache(name)
        return True

if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser()
    parser.add_option('-f', '--force', action="store_true", dest='force', default=False)
    parser.add_option('-x', '--exclude', dest='excluded', action='append', default=[])
    parser.add_option('-o', '--output', dest='output')
    parser.add_option('-n', '--name', dest='name', default='clar')
    parser.add_option('-p', '--prefix', dest='prefix', default='test')

    options, args = parser.parse_args()
    if len(args) > 1:
        print("More than one path given")
        sys.exit(1)

    path = args.pop() if args else '.'
    output = options.output or path
    suite = TestSuite(path, output)
    suite.load(options.name, options.prefix, options.force)
    suite.disable(options.excluded)
    if suite.write(options.name):
        print("Written `%s.suite` (%d tests in %d suites)" % (options.name, suite.callback_count(), suite.suite_count()))
