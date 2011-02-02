from waflib.Context import Context
from waflib.Build import BuildContext, CleanContext, \
        InstallContext, UninstallContext

# Unix flags
CFLAGS_UNIX = ["-O2", "-Wall", "-Wextra"]
CFLAGS_UNIX_DBG = ['-g']

# Windows MSVC flags
CFLAGS_WIN32_COMMON = ['/TC', '/W4', '/WX', '/nologo', '/Zi']
CFLAGS_WIN32_RELEASE = ['/O2', '/MD']

# Note: /RTC* cannot be used with optimization on.
CFLAGS_WIN32_DBG = ['/Od', '/RTC1', '/RTCc', '/DEBUG', '/MDd']
CFLAGS_WIN32_L = ['/RELEASE']  # used for /both/ debug and release builds.
                               # sets the module's checksum in the header.
CFLAGS_WIN32_L_DBG = ['/DEBUG']

ALL_LIBS = ['z', 'crypto', 'pthread']

def options(opt):
    opt.load('compiler_c')
    opt.add_option('--sha1', action='store', default='builtin',
        help="Use the builtin SHA1 routines (builtin), the \
PPC optimized version (ppc) or the SHA1 functions from OpenSSL (openssl)")
    opt.add_option('--debug', action='store_true', default=False,
        help='Compile with debug symbols')
    opt.add_option('--msvc', action='store', default=None,
        help='Force a specific MSVC++ version (7.1, 8.0, 9.0, 10.0), if more than one is installed')
    opt.add_option('--arch', action='store', default='x86',
        help='Select target architecture (ia64, x64, x86, x86_amd64, x86_ia64)')

def configure(conf):

    # load the MSVC configuration flags
    if conf.options.msvc:
        conf.env['MSVC_VERSIONS'] = ['msvc ' + conf.options.msvc]

    conf.env['MSVC_TARGETS'] = [conf.options.arch]

    # default configuration for C programs
    conf.load('compiler_c')

    dbg = conf.options.debug
    zlib_name = 'z'

    conf.env.CFLAGS = CFLAGS_UNIX + (CFLAGS_UNIX_DBG if dbg else [])

    if conf.env.DEST_OS == 'win32':
        conf.env.PLATFORM = 'win32'

        if conf.env.CC_NAME == 'msvc':
            conf.env.CFLAGS = CFLAGS_WIN32_COMMON + \
              (CFLAGS_WIN32_DBG if dbg else CFLAGS_WIN32_RELEASE)
            conf.env.LINKFLAGS += CFLAGS_WIN32_L + \
              (CFLAGS_WIN32_L_DBG if dbg else [])
            conf.env.DEFINES += ['WIN32', '_DEBUG', '_LIB', 'ZLIB_WINAPI']
            zlib_name = 'zlibwapi'

        elif conf.env.CC_NAME == 'gcc':
            conf.check(features='c cprogram', lib='pthread', uselib_store='pthread')

    else:
        conf.env.PLATFORM = 'unix'

    # check for Z lib
    conf.check(features='c cprogram', lib=zlib_name, uselib_store='z', install_path=None)

    if conf.options.sha1 not in ['openssl', 'ppc', 'builtin']:
        ctx.fatal('Invalid SHA1 option')

    # check for libcrypto (openssl) if we are using its SHA1 functions
    if conf.options.sha1 == 'openssl':
        conf.check_cfg(package='libcrypto', args=['--cflags', '--libs'], uselib_store='crypto')
        conf.env.DEFINES += ['OPENSSL_SHA1']

    elif conf.options.sha1 == 'ppc':
        conf.env.DEFINES += ['PPC_SHA1']

    conf.env.sha1 = conf.options.sha1

def build(bld):

    # command '[build|clean|install|uninstall]-static'
    if bld.variant == 'static':
        build_library(bld, 'static')

    # command '[build|clean|install|uninstall]-shared'
    elif bld.variant == 'shared':
        build_library(bld, 'shared')

    # command '[build|clean]-tests'
    elif bld.variant == 'test':
        build_library(bld, 'objects')
        build_test(bld)

    # command 'build|clean|install|uninstall': by default, run
    # the same command for both the static and the shared lib
    else:
        from waflib import Options
        Options.commands = [bld.cmd + '-shared', bld.cmd + '-static'] + Options.commands

def build_library(bld, build_type):

    BUILD = {
        'shared' : bld.shlib,
        'static' : bld.stlib,
        'objects' : bld.objects
    }

    directory = bld.path
    sources = directory.ant_glob('src/*.c')

    # Compile platform-dependant code
    # E.g.  src/unix/*.c
    #       src/win32/*.c
    sources = sources + directory.ant_glob('src/%s/*.c' % bld.env.PLATFORM)

    # SHA1 methods source
    if bld.env.sha1 == "ppc":
        sources.append('src/ppc/sha1.c')
    else:
        sources.append('src/block-sha1/sha1.c')
    #------------------------------
    # Build the main library
    #------------------------------

    # either as static or shared;
    BUILD[build_type](
        source=sources,
        target='git2',
        includes='src',
        install_path='${LIBDIR}',
        use=ALL_LIBS
    )

    # On Unix systems, build the Pkg-config entry file
    if bld.env.PLATFORM == 'unix' and bld.is_install:
        bld(rule="""sed -e 's#@prefix@#${PREFIX}#' -e 's#@libdir@#${LIBDIR}#' < ${SRC} > ${TGT}""",
            source='libgit2.pc.in',
            target='libgit2.pc',
            install_path='${LIBDIR}/pkgconfig',
        )

    # Install headers
    bld.install_files('${PREFIX}/include', directory.find_node('src/git2.h'))
    bld.install_files('${PREFIX}/include/git2', directory.ant_glob('src/git2/*.h'))

    # On Unix systems, let them know about installation
    if bld.env.PLATFORM == 'unix' and bld.cmd == 'install-shared':
        bld.add_post_fun(call_ldconfig)

def call_ldconfig(bld):
    import distutils.spawn as s
    ldconf = s.find_executable('ldconfig')
    if ldconf:
        bld.exec_command(ldconf)

def build_test(bld):
    directory = bld.path
    resources_path = directory.find_node('tests/resources/').abspath().replace('\\', '/')

    sources = ['tests/test_lib.c', 'tests/test_helpers.c', 'tests/test_main.c']
    sources = sources + directory.ant_glob('tests/t??-*.c')

    bld.program(
        source=sources,
        target='libgit2_test',
        includes=['src', 'tests'],
        defines=['TEST_RESOURCES="%s"' % resources_path],
        use=['git2'] + ALL_LIBS
    )

class _test(BuildContext):
    cmd = 'test'
    fun = 'test'

def test(bld):
    from waflib import Options
    Options.commands = ['build-test', 'run-test'] + Options.commands

class _build_doc(Context):
    cmd = 'doxygen'
    fun = 'build_docs'

def build_docs(ctx):
    ctx.exec_command("doxygen api.doxygen")
    ctx.exec_command("git stash")
    ctx.exec_command("git checkout gh-pages")
    ctx.exec_command("cp -Rf apidocs/html/* .")
    ctx.exec_command("git add .")
    ctx.exec_command("git commit -am 'generated docs'")
    ctx.exec_command("git push origin gh-pages")
    ctx.exec_command("git checkout master")

class _run_test(Context):
    cmd = 'run-test'
    fun = 'run_test'

def run_test(ctx):
    import shutil, tempfile, sys

    failed = False

    test_path = 'build/test/libgit2_test'
    if sys.platform == 'win32':
        test_path += '.exe'

    test_folder = tempfile.mkdtemp()
    test = ctx.path.find_node(test_path)

    if not test or ctx.exec_command(test.abspath(), cwd=test_folder) != 0:
        failed = True

    shutil.rmtree(test_folder)

    if failed:
        ctx.fatal('Test run failed')


CONTEXTS = {
    'build'     : BuildContext,
    'clean'     : CleanContext,
    'install'   : InstallContext,
    'uninstall' : UninstallContext
}

def build_command(command):
    ctx, var = command.split('-')
    class _gen_command(CONTEXTS[ctx]):
        cmd = command
        variant = var

build_command('build-static')
build_command('build-shared')
build_command('build-test')

build_command('clean-static')
build_command('clean-shared')
build_command('clean-test')

build_command('install-static')
build_command('install-shared')

build_command('uninstall-static')
build_command('uninstall-shared')

