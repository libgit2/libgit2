#!/bin/bash

# M4 Required with Xcode beta:
export M4=$(xcrun -f m4)
OSX_SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
IOS_SDKROOT=$(xcrun --sdk iphoneos --show-sdk-path)
SIM_SDKROOT=$(xcrun --sdk iphonesimulator --show-sdk-path)

source_dir=$PWD
frameworks_dir=$PWD

# proj dos not use SYSROOT or CFLAGS with cmake, so we use configure

mkdir -p build-osx
pushd build-osx
cmake $source_dir \
	-DREGEX_BACKEND=builtin -DBUILD_CLAR=OFF -DBUILD_EXAMPLES=ON -DBUILD_SHARED_LIBS=OFF -DTHREADSAFE=OFF \
	-DCMAKE_INSTALL_PREFIX=@rpath \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_SYSROOT=${OSX_SDKROOT} \
	-DCMAKE_C_COMPILER=$(xcrun --sdk macosx -f clang) \
	-DCMAKE_C_FLAGS="-I${frameworks_dir}/Frameworks_macosx/libssh2.framework/Headers" \
	-DCMAKE_MODULE_LINKER_FLAGS="-F${frameworks_dir}/Frameworks_macosx " \
	-DCMAKE_SHARED_LINKER_FLAGS="-F${frameworks_dir}/Frameworks_macosx " \
	-DCMAKE_EXE_LINKER_FLAGS="-F${frameworks_dir}/Frameworks_macosx " \
	-DCMAKE_LIBRARY_PATH=${OSX_SDKROOT}/lib/ \
	-DCMAKE_INCLUDE_PATH=${OSX_SDKROOT}/include/ 
make
popd

mkdir -p build-iphoneos
pushd build-iphoneos
cmake $source_dir \
	-DREGEX_BACKEND=builtin -DBUILD_CLAR=OFF -DBUILD_EXAMPLES=ON -DBUILD_SHARED_LIBS=OFF -DTHREADSAFE=OFF\
	-DCMAKE_INSTALL_PREFIX=@rpath \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_SYSROOT=${IOS_SDKROOT} \
	-DCMAKE_C_COMPILER=$(xcrun --sdk iphoneos -f clang) \
	-DCMAKE_C_FLAGS="-arch arm64 -target arm64-apple-darwin19.6.0 -O2 -miphoneos-version-min=14 -I${frameworks_dir}/Frameworks_iphoneos/libssh2.framework/Headers -I${source_dir}" \
	-DCMAKE_MODULE_LINKER_FLAGS="-arch arm64 -target arm64-apple-darwin19.6.0 -O2 -miphoneos-version-min=14 -F${frameworks_dir}/Frameworks_iphoneos " \
	-DCMAKE_SHARED_LINKER_FLAGS="-arch arm64 -target arm64-apple-darwin19.6.0 -O2 -miphoneos-version-min=14 -F${frameworks_dir}/Frameworks_iphoneos " \
	-DCMAKE_EXE_LINKER_FLAGS="-arch arm64 -target arm64-apple-darwin19.6.0 -O2 -miphoneos-version-min=14 -F${frameworks_dir}/Frameworks_iphoneos -framework ios_system" \
	-DCMAKE_LIBRARY_PATH=${IOS_SDKROOT}/lib/ \
	-DCMAKE_INCLUDE_PATH=${IOS_SDKROOT}/include/ 
make
pushd examples
# We need a dynamic library, not an executable:
ld -arch arm64 -dylib -syslibroot ${IOS_SDKROOT} -F${frameworks_dir}/Frameworks_iphoneos CMakeFiles/lg2.dir/add.c.o CMakeFiles/lg2.dir/args.c.o CMakeFiles/lg2.dir/blame.c.o CMakeFiles/lg2.dir/cat-file.c.o CMakeFiles/lg2.dir/checkout.c.o CMakeFiles/lg2.dir/clone.c.o CMakeFiles/lg2.dir/commit.c.o CMakeFiles/lg2.dir/common.c.o CMakeFiles/lg2.dir/config.c.o CMakeFiles/lg2.dir/describe.c.o CMakeFiles/lg2.dir/diff.c.o CMakeFiles/lg2.dir/fetch.c.o CMakeFiles/lg2.dir/for-each-ref.c.o CMakeFiles/lg2.dir/general.c.o CMakeFiles/lg2.dir/index-pack.c.o CMakeFiles/lg2.dir/init.c.o CMakeFiles/lg2.dir/lg2.c.o CMakeFiles/lg2.dir/log.c.o CMakeFiles/lg2.dir/ls-files.c.o CMakeFiles/lg2.dir/ls-remote.c.o CMakeFiles/lg2.dir/merge.c.o CMakeFiles/lg2.dir/path.c.o CMakeFiles/lg2.dir/push.c.o CMakeFiles/lg2.dir/pull.c.o CMakeFiles/lg2.dir/branch.c.o CMakeFiles/lg2.dir/remote.c.o CMakeFiles/lg2.dir/reset.c.o CMakeFiles/lg2.dir/rev-list.c.o CMakeFiles/lg2.dir/rev-parse.c.o CMakeFiles/lg2.dir/show-index.c.o CMakeFiles/lg2.dir/stash.c.o CMakeFiles/lg2.dir/status.c.o CMakeFiles/lg2.dir/tag.c.o -o lg2.dylib ../libgit2.a -lpthread -framework CoreFoundation -framework Security -framework libssh2 -framework openssl -framework ios_system -liconv -lz
popd
popd

mkdir -p build-iphonesimulator
pushd build-iphonesimulator
cmake $source_dir \
	-DREGEX_BACKEND=builtin -DBUILD_CLAR=OFF -DBUILD_EXAMPLES=ON -DBUILD_SHARED_LIBS=OFF -DTHREADSAFE=OFF\
	-DCMAKE_INSTALL_PREFIX=@rpath \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_OSX_SYSROOT=${SIM_SDKROOT} \
	-DCMAKE_C_COMPILER=$(xcrun --sdk iphonesimulator -f clang) \
	-DCMAKE_C_FLAGS="-target x86_64-apple-darwin19.6.0 -O2 -mios-simulator-version-min=14.0 -I${frameworks_dir}/Frameworks_iphonesimulator/libssh2.framework/Headers -I${source_dir}" \
	-DCMAKE_MODULE_LINKER_FLAGS="-dylib -target x86_64-apple-darwin19.6.0 -O2 -mios-simulator-version-min=14.0 -F${frameworks_dir}/Frameworks_iphonesimulator " \
	-DCMAKE_SHARED_LINKER_FLAGS="-dylib -target x86_64-apple-darwin19.6.0 -O2 -mios-simulator-version-min=14.0 -F${frameworks_dir}/Frameworks_iphonesimulator " \
	-DCMAKE_EXE_LINKER_FLAGS="-dylib -target x86_64-apple-darwin19.6.0 -O2 -mios-simulator-version-min=14.0 -F${frameworks_dir}/Frameworks_iphonesimulator -framework ios_system" \
	-DCMAKE_LIBRARY_PATH=${SIM_SDKROOT}/lib/ \
	-DCMAKE_INCLUDE_PATH=${SIM_SDKROOT}/include/ 
make
pushd examples
# We need a dynamic library, not an executable:
ld -arch x86_64 -dylib -syslibroot ${SIM_SDKROOT} -F${frameworks_dir}/Frameworks_iphonesimulator CMakeFiles/lg2.dir/add.c.o CMakeFiles/lg2.dir/args.c.o CMakeFiles/lg2.dir/blame.c.o CMakeFiles/lg2.dir/cat-file.c.o CMakeFiles/lg2.dir/checkout.c.o CMakeFiles/lg2.dir/clone.c.o CMakeFiles/lg2.dir/commit.c.o CMakeFiles/lg2.dir/common.c.o CMakeFiles/lg2.dir/config.c.o CMakeFiles/lg2.dir/describe.c.o CMakeFiles/lg2.dir/diff.c.o CMakeFiles/lg2.dir/fetch.c.o CMakeFiles/lg2.dir/for-each-ref.c.o CMakeFiles/lg2.dir/general.c.o CMakeFiles/lg2.dir/index-pack.c.o CMakeFiles/lg2.dir/init.c.o CMakeFiles/lg2.dir/lg2.c.o CMakeFiles/lg2.dir/log.c.o CMakeFiles/lg2.dir/ls-files.c.o CMakeFiles/lg2.dir/ls-remote.c.o CMakeFiles/lg2.dir/merge.c.o CMakeFiles/lg2.dir/path.c.o CMakeFiles/lg2.dir/push.c.o CMakeFiles/lg2.dir/pull.c.o CMakeFiles/lg2.dir/branch.c.o CMakeFiles/lg2.dir/remote.c.o CMakeFiles/lg2.dir/reset.c.o CMakeFiles/lg2.dir/rev-list.c.o CMakeFiles/lg2.dir/rev-parse.c.o CMakeFiles/lg2.dir/show-index.c.o CMakeFiles/lg2.dir/stash.c.o CMakeFiles/lg2.dir/status.c.o CMakeFiles/lg2.dir/tag.c.o -o lg2.dylib ../libgit2.a -lpthread -framework CoreFoundation -framework Security -framework libssh2 -framework openssl -framework ios_system -liconv -lz
popd
popd

# First create frameworks, then change I/O inside lg2 (and libgit2?)

# Now create frameworks:
for platform in iphoneos iphonesimulator
do 
	for binary in lg2
	do
		FRAMEWORK_DIR=build/Release-${platform}/${binary}.framework
		rm -rf ${FRAMEWORK_DIR}
		mkdir -p ${FRAMEWORK_DIR}
		cp build-$platform/examples/$binary.dylib ${FRAMEWORK_DIR}/$binary
		if [ "$platform" == "iphoneos" ]; then
			cp basic_Info.plist ${FRAMEWORK_DIR}/Info.plist
		elif [ "$platform" == "iphonesimulator" ]; then
			cp basic_Info_Simulator.plist ${FRAMEWORK_DIR}/Info.plist
		else 
			cp basic_Info_OSX.plist ${FRAMEWORK_DIR}/Info.plist
		fi
		plutil -replace CFBundleExecutable -string $binary ${FRAMEWORK_DIR}/Info.plist
		plutil -replace CFBundleName -string $binary ${FRAMEWORK_DIR}/Info.plist
		plutil -replace CFBundleIdentifier -string Nicolas-Holzschuch.$binary  ${FRAMEWORK_DIR}/Info.plist
		install_name_tool -id @rpath/$binary.framework/$binary   ${FRAMEWORK_DIR}/$binary
	done
done

framework=lg2
rm -rf $framework.xcframework
xcodebuild -create-xcframework -framework build/Release-iphoneos/$framework.framework -framework build/Release-iphonesimulator/$framework.framework -output $framework.xcframework

