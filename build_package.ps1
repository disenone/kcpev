param($folder, $gen, $config, $extra_cmake)

cd $folder
md build -Force | Out-Null
cd build 
if ($gen){
    & cmake $extra_cmake -G "$gen" ..
}
else{
    & cmake $extra_cmake ..
}
if ($LastExitCode -ne 0) {
    throw "Exec: $ErrorMessage"
}
if ($config){
    & cmake --build . --config $config
}
else{
    & cmake --build .
}
if ($LastExitCode -ne 0) {
    throw "Exec: $ErrorMessage"
}

