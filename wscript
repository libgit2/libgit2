CFLAGS = ["-g", "-O2", "-Wall", "-Wextra"]

def options(opt):
	opt.load('compiler_c')
	opt.add_option('--sha1', action='store', default='builtin', help='TODO')

def configure(conf):
	conf.load('compiler_c')

	if conf.options.sha1 not in ['openssh', 'ppc', 'builtin']:
		ctx.fatal('Invalid SHA1 option')

	conf.env.sha1 = conf.options.sha1


def build(bld):
	import glob, sys

	sources = glob.glob('src/*.c')
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

	sources = sources + glob.glob('src/%s/*.c' % os)

	bld.stlib(
		source=sources,
		target='git2',
		includes='src',
		cflags=flags,
		defines=defines
	)

	bld.shlib(
		source=sources,
		target='git2',
		includes='src',
		cflags=flags,
		defines=defines
	)

	bld.install_files('${PREFIX}/include/git', glob.glob('src/git/*.h'))

