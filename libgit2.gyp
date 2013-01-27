# HOW TO USE
#
# Generate Makefile:
# gyp libgit2.gyp --depth . -I common.gypi
#
# Compile:
# make libgit2
#
# Compile with Release mode:
# make libgit2 BUILDTYPE=Release
#
# Compile and show actual commands:
# make libgit2 V=1
#
# TODO
# * [WIN][MAC] thread safe libraries
# * [WIN] detect mingw
# * [POSIX] detect openssl (always assume its existence now)
# * [ALL] detect zlib (always assume it doesn't exist now)
# * [ALL] install
{
  'variables': {
    'libgit2_build_type%': 'static_library',
    'libgit2_thread_safe%': 'false',
    'libgit2_system_zlib%': 'false',
    'libgit2_use_openssl%': 'true',
    'libgit2_profile%': 'false',
    'libgit2_mingw%': 'false',
    'libgit2_cygwin%': 'false',
    'libgit2_stdcall%': 'true',
  },

  'targets': [
    {
      'target_name': 'libgit2',
      'type': '<(libgit2_build_type)',

      'include_dirs': [
        'src',
        'include',
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },

      'sources': [
        'src/attr.c',
        'src/attr_file.c',
        'src/attr_file.h',
        'src/attr.h',
        'src/blob.c',
        'src/blob.h',
        'src/branch.c',
        'src/branch.h',
        'src/bswap.h',
        'src/buffer.c',
        'src/buffer.h',
        'src/buf_text.c',
        'src/buf_text.h',
        'src/cache.c',
        'src/cache.h',
        'src/cc-compat.h',
        'src/checkout.c',
        'src/checkout.h',
        'src/clone.c',
        'src/commit.c',
        'src/commit.h',
        'src/commit_list.c',
        'src/commit_list.h',
        'src/common.h',
        'src/compress.c',
        'src/compress.h',
        'src/config.c',
        'src/config_cache.c',
        'src/config_file.c',
        'src/config_file.h',
        'src/config.h',
        'src/crlf.c',
        'src/date.c',
        'src/delta-apply.c',
        'src/delta-apply.h',
        'src/delta.c',
        'src/delta.h',
        'src/diff.c',
        'src/diff.h',
        'src/diff_output.c',
        'src/diff_output.h',
        'src/diff_tform.c',
        'src/errors.c',
        'src/fetch.c',
        'src/fetch.h',
        'src/fetchhead.c',
        'src/fetchhead.h',
        'src/filebuf.c',
        'src/filebuf.h',
        'src/fileops.c',
        'src/fileops.h',
        'src/filter.c',
        'src/filter.h',
        'src/fnmatch.c',
        'src/fnmatch.h',
        'src/global.c',
        'src/global.h',
        'src/graph.c',
        'src/hash.c',
        'src/hash.h',
        'src/ignore.c',
        'src/ignore.h',
        'src/index.c',
        'src/indexer.c',
        'src/index.h',
        'src/iterator.c',
        'src/iterator.h',
        'src/khash.h',
        'src/map.h',
        'src/merge.c',
        'src/merge.h',
        'src/message.c',
        'src/message.h',
        'src/mwindow.c',
        'src/mwindow.h',
        'src/netops.c',
        'src/netops.h',
        'src/notes.c',
        'src/notes.h',
        'src/object.c',
        'src/object.h',
        'src/odb.c',
        'src/odb.h',
        'src/odb_loose.c',
        'src/odb_pack.c',
        'src/offmap.h',
        'src/oid.c',
        'src/oidmap.h',
        'src/pack.c',
        'src/pack.h',
        'src/pack-objects.c',
        'src/pack-objects.h',
        'src/path.c',
        'src/path.h',
        'src/pathspec.c',
        'src/pathspec.h',
        'src/pool.c',
        'src/pool.h',
        'src/posix.c',
        'src/posix.h',
        'src/pqueue.c',
        'src/pqueue.h',
        'src/push.c',
        'src/push.h',
        'src/reflog.c',
        'src/reflog.h',
        'src/refs.c',
        'src/refs.h',
        'src/refspec.c',
        'src/refspec.h',
        'src/remote.c',
        'src/remote.h',
        'src/repository.c',
        'src/repository.h',
        'src/repo_template.h',
        'src/reset.c',
        'src/revparse.c',
        'src/revwalk.c',
        'src/revwalk.h',
        'src/sha1_lookup.c',
        'src/sha1_lookup.h',
        'src/signature.c',
        'src/signature.h',
        'src/stash.c',
        'src/status.c',
        'src/strmap.h',
        'src/submodule.c',
        'src/submodule.h',
        'src/tag.c',
        'src/tag.h',
        'src/thread-utils.c',
        'src/thread-utils.h',
        'src/transport.c',
        'src/tree.c',
        'src/tree-cache.c',
        'src/tree-cache.h',
        'src/tree.h',
        'src/tsort.c',
        'src/util.c',
        'src/util.h',
        'src/vector.c',
        'src/vector.h',
        'src/transports/cred.c',
        'src/transports/cred_helpers.c',
        'src/transports/git.c',
        'src/transports/http.c',
        'src/transports/local.c',
        'src/transports/smart.c',
        'src/transports/smart.h',
        'src/transports/smart_pkt.c',
        'src/transports/smart_protocol.c',
        'src/transports/winhttp.c',
        'src/xdiff/xdiff.h',
        'src/xdiff/xdiffi.c',
        'src/xdiff/xdiffi.h',
        'src/xdiff/xemit.c',
        'src/xdiff/xemit.h',
        'src/xdiff/xhistogram.c',
        'src/xdiff/xinclude.h',
        'src/xdiff/xmacros.h',
        'src/xdiff/xmerge.c',
        'src/xdiff/xpatience.c',
        'src/xdiff/xprepare.c',
        'src/xdiff/xprepare.h',
        'src/xdiff/xtypes.h',
        'src/xdiff/xutils.c',
        'src/xdiff/xutils.h',
        # Add headers in IDE.
        'include/git2.h',
        'include/git2/attr.h',
        'include/git2/blob.h',
        'include/git2/branch.h',
        'include/git2/checkout.h',
        'include/git2/clone.h',
        'include/git2/commit.h',
        'include/git2/common.h',
        'include/git2/config.h',
        'include/git2/cred_helpers.h',
        'include/git2/diff.h',
        'include/git2/errors.h',
        'include/git2/graph.h',
        'include/git2/ignore.h',
        'include/git2/indexer.h',
        'include/git2/index.h',
        'include/git2/inttypes.h',
        'include/git2/merge.h',
        'include/git2/message.h',
        'include/git2/net.h',
        'include/git2/notes.h',
        'include/git2/object.h',
        'include/git2/odb_backend.h',
        'include/git2/odb.h',
        'include/git2/oid.h',
        'include/git2/pack.h',
        'include/git2/push.h',
        'include/git2/reflog.h',
        'include/git2/refs.h',
        'include/git2/refspec.h',
        'include/git2/remote.h',
        'include/git2/repository.h',
        'include/git2/reset.h',
        'include/git2/revparse.h',
        'include/git2/revwalk.h',
        'include/git2/signature.h',
        'include/git2/stash.h',
        'include/git2/status.h',
        'include/git2/stdint.h',
        'include/git2/strarray.h',
        'include/git2/submodule.h',
        'include/git2/tag.h',
        'include/git2/threads.h',
        'include/git2/transport.h',
        'include/git2/tree.h',
        'include/git2/types.h',
        'include/git2/version.h',
      ],

      'conditions': [
        [ 'OS=="win" and libgit2_mingw=="false"', {
          'defines': [ 'GIT_WINHTTP' ],
        },{ # POSIX
          'dependencies': [ 'deps/http-parser/http_parser.gyp:http_parser' ]
        }],

        # Specify sha1 implementation.
        [ 'OS=="win" and libgit2_mingw=="false"', {
          'defines': [ 'WIN32_SHA1' ],
          'sources': [ 'src/hash/hash_win32.c' ]
        }],
        [ '(OS!="win" or libgit2_mingw=="true") and libgit2_use_openssl=="true"', {
          'defines': [ 'OPENSSL_SHA1' ]
        }],
        [ '(OS!="win" or libgit2_mingw=="true") and libgit2_use_openssl=="false"', {
          'sources': [ 'src/hash/hash_generic.c' ]
        }],

        # Include POSIX regex when it is required.
        [ 'OS=="win" or OS=="amiga"', {
          'dependencies': [ 'deps/regex/regex.gyp:gnu_regex' ]
        }],

        # Optional external dependency: zlib.
        [ 'libgit2_system_zlib=="false"', {
          'defines': [
	          'NO_VIZ',
            'STDC',
            'NO_GZIP',
          ],
          'dependencies': [ 'deps/zlib/zlib.gyp:zlib' ],
        }],

        # Platform specific compilation flags.
        [ 'libgit2_build_type=="shared_library"', {
          'cflags': [
            '-fvisibility=hidden',
            '-fPIC',
          ]
        }],
        [ 'libgit2_profile=="true"', {
          'cflags': [ '-pg' ],
          'ldflags': [ '-pg' ]
        }],
        [ 'libgit2_stdcall=="true"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '/Gz',
              ],
            }
          }
        }],

        # On Windows use specific platform sources.
        [ 'OS=="win" and libgit2_cygwin=="false"', {
          'defines': [
            'WIN32',
            '_DEBUG',
            '_WIN32_WINNT=0x0501',
          ],
          'sources': [
            'src/win32/dir.c',
            'src/win32/dir.h',
            'src/win32/findfile.c',
            'src/win32/findfile.h',
            'src/win32/map.c',
            'src/win32/mingw-compat.h',
            'src/win32/msvc-compat.h',
            'src/win32/posix.h',
            'src/win32/posix_w32.c',
            'src/win32/precompiled.c',
            'src/win32/precompiled.h',
            'src/win32/pthread.c',
            'src/win32/pthread.h',
            'src/win32/utf-conv.c',
            'src/win32/utf-conv.h',
            'src/win32/git2.rc',
          ]
        }],
        [ 'OS=="amiga"', {
          'defines': [
            'NO_ADDRINFO',
            'NO_READDIR_R',
          ],
          'sources': [ 'src/amiga/map.c' ]
        }],
        [ '(OS!="win" or libgit2_cygwin=="true") and OS!="amiga"', { # UNIX
          'sources': [ 'src/unix/map.c' ]
        }],
      ]
    },

    {
      'target_name': 'libgit2_clar',
      'type': 'executable',
      'dependencies': [ 'libgit2' ],
      'include_dirs': [
        'tests-clar',
        'src',
      ],
      'conditions': [
        [ 'OS=="win" and libgit2_mingw=="false"', {
          'defines': [ 'WIN32_SHA1' ]
        }],
        [ '(OS!="win" or libgit2_mingw=="true") and libgit2_use_openssl=="true"', {
          'defines': [ 'OPENSSL_SHA1' ]
        }],
      ],
      'defines': [
        # TODO should use script to generate pwd instead of using shell command.
        'CLAR_FIXTURE_PATH="<!@(pwd)/tests-clar/resources/"'
      ],
      'sources': [
        'tests-clar/clar_libgit2.c',
        'tests-clar/fetchhead/fetchhead_data.h',
        'tests-clar/fetchhead/nonetwork.c',
        'tests-clar/attr/flags.c',
        'tests-clar/attr/file.c',
        'tests-clar/attr/lookup.c',
        'tests-clar/attr/repo.c',
        'tests-clar/attr/attr_expect.h',
        'tests-clar/threads/basic.c',
        'tests-clar/repo/message.c',
        'tests-clar/repo/repo_helpers.c',
        'tests-clar/repo/repo_helpers.h',
        'tests-clar/repo/init.c',
        'tests-clar/repo/head.c',
        'tests-clar/repo/hashfile.c',
        'tests-clar/repo/getters.c',
        'tests-clar/repo/headtree.c',
        'tests-clar/repo/state.c',
        'tests-clar/repo/setters.c',
        'tests-clar/repo/open.c',
        'tests-clar/repo/discover.c',
        'tests-clar/status/single.c',
        'tests-clar/status/status_helpers.h',
        'tests-clar/status/worktree.c',
        'tests-clar/status/ignore.c',
        'tests-clar/status/status_data.h',
        'tests-clar/status/worktree_init.c',
        'tests-clar/status/submodules.c',
        'tests-clar/status/status_helpers.c',
        'tests-clar/reset/reset_helpers.h',
        'tests-clar/reset/mixed.c',
        'tests-clar/reset/soft.c',
        'tests-clar/reset/reset_helpers.c',
        'tests-clar/reset/hard.c',
        'tests-clar/clar_libgit2.h',
        'tests-clar/network/refspecs.c',
        'tests-clar/network/remoterename.c',
        'tests-clar/network/cred.c',
        'tests-clar/network/remotelocal.c',
        'tests-clar/network/fetchlocal.c',
        'tests-clar/network/createremotethenload.c',
        'tests-clar/network/remotes.c',
        'tests-clar/clone/empty.c',
        'tests-clar/clone/nonetwork.c',
        'tests-clar/clar/fs.h',
        'tests-clar/clar/fixtures.h',
        'tests-clar/clar/print.h',
        'tests-clar/clar/sandbox.h',
        'tests-clar/main.c',
        'tests-clar/online/push.c',
        'tests-clar/online/push_util.h',
        'tests-clar/online/fetch.c',
        'tests-clar/online/push_util.c',
        'tests-clar/online/clone.c',
        'tests-clar/online/fetchhead.c',
        'tests-clar/commit/signature.c',
        'tests-clar/commit/commit.c',
        'tests-clar/commit/write.c',
        'tests-clar/commit/parent.c',
        'tests-clar/commit/parse.c',
        'tests-clar/diff/workdir.c',
        'tests-clar/diff/index.c',
        'tests-clar/diff/rename.c',
        'tests-clar/diff/blob.c',
        'tests-clar/diff/diffiter.c',
        'tests-clar/diff/patch.c',
        'tests-clar/diff/iterator.c',
        'tests-clar/diff/tree.c',
        'tests-clar/diff/diff_helpers.c',
        'tests-clar/diff/diff_helpers.h',
        'tests-clar/buf/basic.c',
        'tests-clar/buf/splice.c',
        'tests-clar/object/peel.c',
        'tests-clar/object/message.c',
        'tests-clar/object/blob/fromchunks.c',
        'tests-clar/object/blob/write.c',
        'tests-clar/object/blob/filter.c',
        'tests-clar/object/lookup.c',
        'tests-clar/object/raw/hash.c',
        'tests-clar/object/raw/fromstr.c',
        'tests-clar/object/raw/type2string.c',
        'tests-clar/object/raw/write.c',
        'tests-clar/object/raw/data.h',
        'tests-clar/object/raw/size.c',
        'tests-clar/object/raw/compare.c',
        'tests-clar/object/raw/chars.c',
        'tests-clar/object/raw/convert.c',
        'tests-clar/object/raw/short.c',
        'tests-clar/object/commit/commitstagedfile.c',
        'tests-clar/object/tree/duplicateentries.c',
        'tests-clar/object/tree/walk.c',
        'tests-clar/object/tree/read.c',
        'tests-clar/object/tree/write.c',
        'tests-clar/object/tree/frompath.c',
        'tests-clar/object/tree/attributes.c',
        'tests-clar/object/tag/list.c',
        'tests-clar/object/tag/peel.c',
        'tests-clar/object/tag/read.c',
        'tests-clar/object/tag/write.c',
        'tests-clar/merge/setup.c',
        'tests-clar/pack/packbuilder.c',
        'tests-clar/notes/notes.c',
        'tests-clar/notes/notesref.c',
        'tests-clar/core/strmap.c',
        'tests-clar/core/hex.c',
        'tests-clar/core/pool.c',
        'tests-clar/core/copy.c',
        'tests-clar/core/path.c',
        'tests-clar/core/strtol.c',
        'tests-clar/core/rmdir.c',
        'tests-clar/core/vector.c',
        'tests-clar/core/buffer.c',
        'tests-clar/core/dirent.c',
        'tests-clar/core/errors.c',
        'tests-clar/core/stat.c',
        'tests-clar/core/oid.c',
        'tests-clar/core/env.c',
        'tests-clar/core/filebuf.c',
        'tests-clar/core/mkdir.c',
        'tests-clar/core/string.c',
        'tests-clar/index/tests.c',
        'tests-clar/index/read_tree.c',
        'tests-clar/index/rename.c',
        'tests-clar/index/conflicts.c',
        'tests-clar/index/reuc.c',
        'tests-clar/index/filemodes.c',
        'tests-clar/index/inmemory.c',
        'tests-clar/index/stage.c',
        'tests-clar/stash/save.c',
        'tests-clar/stash/foreach.c',
        'tests-clar/stash/stash_helpers.h',
        'tests-clar/stash/drop.c',
        'tests-clar/stash/stash_helpers.c',
        'tests-clar/clar.c',
        'tests-clar/date/date.c',
        'tests-clar/refs/list.c',
        'tests-clar/refs/peel.c',
        'tests-clar/refs/reflog/reflog.c',
        'tests-clar/refs/reflog/drop.c',
        'tests-clar/refs/listall.c',
        'tests-clar/refs/delete.c',
        'tests-clar/refs/foreachglob.c',
        'tests-clar/refs/rename.c',
        'tests-clar/refs/crashes.c',
        'tests-clar/refs/read.c',
        'tests-clar/refs/lookup.c',
        'tests-clar/refs/revparse.c',
        'tests-clar/refs/pack.c',
        'tests-clar/refs/overwrite.c',
        'tests-clar/refs/update.c',
        'tests-clar/refs/normalize.c',
        'tests-clar/refs/unicode.c',
        'tests-clar/refs/branches/delete.c',
        'tests-clar/refs/branches/move.c',
        'tests-clar/refs/branches/foreach.c',
        'tests-clar/refs/branches/tracking.c',
        'tests-clar/refs/branches/lookup.c',
        'tests-clar/refs/branches/ishead.c',
        'tests-clar/refs/branches/trackingname.c',
        'tests-clar/refs/branches/create.c',
        'tests-clar/refs/create.c',
        'tests-clar/refs/isvalidname.c',
        'tests-clar/clar.h',
        'tests-clar/checkout/checkout_helpers.c',
        'tests-clar/checkout/index.c',
        'tests-clar/checkout/typechange.c',
        'tests-clar/checkout/head.c',
        'tests-clar/checkout/binaryunicode.c',
        'tests-clar/checkout/crlf.c',
        'tests-clar/checkout/tree.c',
        'tests-clar/checkout/checkout_helpers.h',
        'tests-clar/revwalk/mergebase.c',
        'tests-clar/revwalk/signatureparsing.c',
        'tests-clar/revwalk/basic.c',
        'tests-clar/submodule/submodule_helpers.c',
        'tests-clar/submodule/lookup.c',
        'tests-clar/submodule/submodule_helpers.h',
        'tests-clar/submodule/status.c',
        'tests-clar/submodule/modify.c',
        'tests-clar/config/add.c',
        'tests-clar/config/config_helpers.c',
        'tests-clar/config/refresh.c',
        'tests-clar/config/read.c',
        'tests-clar/config/write.c',
        'tests-clar/config/stress.c',
        'tests-clar/config/new.c',
        'tests-clar/config/configlevel.c',
        'tests-clar/config/multivar.c',
        'tests-clar/config/backend.c',
        'tests-clar/config/config_helpers.h',
        'tests-clar/odb/loose.c',
        'tests-clar/odb/pack_data.h',
        'tests-clar/odb/pack_data_one.h',
        'tests-clar/odb/foreach.c',
        'tests-clar/odb/mixed.c',
        'tests-clar/odb/packed_one.c',
        'tests-clar/odb/sorting.c',
        'tests-clar/odb/alternates.c',
        'tests-clar/odb/packed.c',
        'tests-clar/odb/loose_data.h',
      ],
    },

    {
      'target_name': 'libgit2_examples',
      'type': 'none',
      'dependencies': [
        'cgit2',
        'git-diff',
        'git-general',
        'git-showindex',
      ]
    },

    {
      'target_name': 'cgit2',
      'type': 'executable',
      'ldflags': [ '-pthread', ],
      'sources': [
        'examples/network/clone.c',
        'examples/network/common.h',
        'examples/network/fetch.c',
        'examples/network/git2.c',
        'examples/network/index-pack.c',
        'examples/network/ls-remote.c',
      ],
      'dependencies': [ 'libgit2' ]
    },

    {
      'target_name': 'git-diff',
      'type': 'executable',
      'sources': [ 'examples/diff.c' ],
      'dependencies': [ 'libgit2' ]
    },

    {
      'target_name': 'git-general',
      'type': 'executable',
      'sources': [ 'examples/general.c' ],
      'dependencies': [ 'libgit2' ]
    },

    {
      'target_name': 'git-showindex',
      'type': 'executable',
      'sources': [ 'examples/showindex.c' ],
      'dependencies': [ 'libgit2' ]
    },
  ], # end targets

  'target_defaults': {
    'target_conditions': [
      ['_target_name=="libgit2" or _target_name=="libgit2_clar"', {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'ws2_32.lib',
            ],
          }
        },

        'conditions': [
          [ 'libgit2_use_openssl=="true"', {
            'defines': [ 'GIT_SSL' ],
            'ldflags': [ '<!@(pkg-config --libs openssl)' ],
          }],

          [ 'libgit2_thread_safe=="true"', {
            'defines': [ 'GIT_THREADS' ],
            'conditions': [
              [ 'OS=="win"', {
              }],
              [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
                'ldflags': [ '-pthread', ],
                'conditions': [
                  [ 'OS=="solaris"', {
                    'cflags': [ '-pthreads' ],
                    'ldflags': [ '-pthreads' ],
                    'cflags!': [ '-pthread' ],
                    'ldflags!': [ '-pthread' ],
                  }],
                ],
              }],
            ]
          }],

          [ 'OS=="solaris"', {
            'link_settings': {
              'libraries': [
                '-lsocket',
                '-lnsl',
              ]
            }
          }],
        ]
      }]
    ]
  }
}
