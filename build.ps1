# build in windows

param($gen, $config)
$root = (Get-Item -Path ".\" -Verbose).FullName

git submodule update --init --recursive

./build_package.ps1 third_party/googletest $gen $config -extra_cmake "-DBUILD_SHARED_LIBS=ON"
cd $root

./build_package.ps1 third_party/libev-win $gen $config
cd $root

./build_package.ps1 "" $gen $config

