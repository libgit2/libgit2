from waflib.Context import Context
from waflib.Build import BuildContext, CleanContext, \
        InstallContext, UninstallContext

CFLAGS = ["-g", "-O2", "-Wall", "-Wextra"]

def options(opt):
	opt.load('compiler_c')
	opt.add_option('--sha1', action='store', default='builtin',
		help="Use the builtin SHA1 routines (--sha1=builtin), the\
PPC optimized version (--sha1=ppc) or the SHA1 functions from OpenSSH (--sha1=openssh)")

def configure(conf):
	conf.load('compiler_c')

	if conf.options.sha1 not in ['openssh', 'ppc', 'builtin']:
		ctx.fatal('Invalid SHA1 option')

	conf.env.sha1 = conf.options.sha1

def build(bld):
	import sys

	libs = { 'static' : 'cstlib', 'shared' : 'cshlib' }

	if bld.variant not in libs:
		from waflib import Options
		Options.commands = [bld.cmd + '-shared', bld.cmd + '-static'] + Options.commands
		return

	directory = bld.path

	sources = directory.ant_glob('src/*.c')
	flags = CFLAGS
	defines = []
	visibility = True
	os = 'unix'

	if sys.platform == 'win32':
		# windows configuration
		flags = flags + ['-TC', '-W4', '-RTC1', '-Zi']
		defines = defines = ['WIN32', '_DEBUG', '_LIB']
		visibility = False
		os = 'win32'

	elif sys.platform == 'cygwin':
		visibility = False

	elif sys.platform == 'mingw': # TODO
		pass

	if bld.env.sha1 == "openssh":
		defines.append('OPENSSL_SHA1')

	elif bld.env.sha1 == "ppc":
		defines.append('PPC_SHA1')
		sources.append('src/ppc/sha1.c')

	else:
		sources.append('src/block-sha1/sha1.c')

	if not visibility:
		flags.append('-fvisibility=hidden')

	sources = sources + directory.ant_glob('src/%s/*.c' % os)

	bld(features='c ' + libs[bld.variant],
		source=sources,
		target='git2',
		includes='src',
		cflags=flags,
		defines=defines,
		inst_to='${LIBDIR}'
	)

	bld.install_files('${PREFIX}/include/git', directory.ant_glob('src/git/*.h'))
	if os == "unix":
		bld.install_files('${PREFIX}/lib/pkgconfig', 'libgit2.pc')


class _test(BuildContext):
	cmd = 'test'
	fun = 'test'

def test(bld):
	from waflib import Options
	Options.commands = ['build-static', 'build-tests', 'run-tests'] + Options.commands

class _build_tests(BuildContext):
	cmd = 'build-tests'
	fun = 'build_tests'
	variant = 'tests'

def build_tests(bld):
	import os

	directory = bld.path
	bld.objects(source=['tests/test_helpers.c', 'tests/test_lib.c'], includes=['src', 'tests'], target='test_helper')

	for test_file in directory.ant_glob('tests/t????-*.c'):
		test_name, _ = os.path.splitext(os.path.basename(test_file.abspath()))

		test_toc_file = directory.make_node('tests/%s.toc' % test_name)
		if bld.cmd == 'clean':
			test_toc_file.delete()
		else:
			test_toc = bld.cmd_and_log(['grep', 'BEGIN_TEST', test_file.abspath()], quiet=True)
			test_toc_file.write(test_toc)

		bld.program(
			source=[test_file, 'tests/test_main.c'],
			target=test_name,
			includes=['src', 'tests'],
			defines=['TEST_TOC="%s.toc"' % test_name],
			stlib=['git2', 'z'],
			stlibpath=[directory.abspath(), 'build'],
			use='test_helper')

class _run_tests(Context):
	cmd = 'run-tests'
	fun = 'run_tests'

def run_tests(ctx):
	test_folder = ctx.path.make_node('tests/tmp/')

	for test in ctx.path.ant_glob('build/tests/t????-*'):
		test_folder.delete()
		test_folder.mkdir()

		if ctx.exec_command(test.abspath(), cwd=test_folder.abspath()) != 0:
			ctx.fatal('Test run failed')
			break

	test_folder.delete()

for var in ('static', 'shared'):
	for ctx in (BuildContext, CleanContext, InstallContext, UninstallContext):
		name = ctx.__name__.replace('Context', '').lower()
		class _genclass(ctx):
			cmd = name + '-' + var
			variant = var

