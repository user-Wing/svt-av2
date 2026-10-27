$ErrorActionPreference='Stop'
. 'C:\Private\ffmpeg-native\deps-full\AV2\activate.ps1' -Quiet
foreach ($arm in 'prod','ref','nodb','check') {
 $shared=if ($arm -eq 'ref') {0} else {1}
 $ways=if ($arm -eq 'ref') {1} else {8}
 $db=if ($arm -eq 'nodb') {0} else {1}
 $keep=if ($arm -eq 'check') {1} else {0}
 $build=if ($arm -eq 'prod') {'build-stack1'} else {"build-stack1-$arm"}
 & $env:AV2_CMAKE -S .\stack1-source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSVT_AV2_WARNINGS_AS_ERRORS=ON -DSVT_AV2_BUILD_SHARED=OFF -DSVT_AV2_BUILD_TESTS=ON -UCMAKE_C_/DM6_TX_CACHE "-DCMAKE_C_FLAGS=/DM6_TX_CACHE=1 /DM6_TX_CACHE_SLOTS=8192 /DM6_FLIP_TX=1 /DM6_JOINT_Y_TX=1 /DM6_ROOT_ASYM=1 /DM6_Y_BOUND=1 /DM6_RDOQ_WIDE=1 /DM6_TREE_RDOQ=1 /DM6_TOOL_REPEAT=1 /DM6_TREE_SHARED=$shared /DM6_TX_CACHE_WAYS=$ways /DM6_DEBLOCK=$db /DM6_TREE_RETAIN_OLD=$keep /DM6_REPEAT_RETAIN_OLD=$keep" > "configure-stack1-$arm.log" 2>&1
 if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
 $expected="CMAKE_C_FLAGS:STRING=/DM6_TX_CACHE=1 /DM6_TX_CACHE_SLOTS=8192 /DM6_FLIP_TX=1 /DM6_JOINT_Y_TX=1 /DM6_ROOT_ASYM=1 /DM6_Y_BOUND=1 /DM6_RDOQ_WIDE=1 /DM6_TREE_RDOQ=1 /DM6_TOOL_REPEAT=1 /DM6_TREE_SHARED=$shared /DM6_TX_CACHE_WAYS=$ways /DM6_DEBLOCK=$db /DM6_TREE_RETAIN_OLD=$keep /DM6_REPEAT_RETAIN_OLD=$keep"
 if ((Get-Content "$build/CMakeCache.txt") -notcontains $expected) {throw "Feature flags were not applied"}
 & $env:AV2_CMAKE --build $build --target SvtAv2EncApp --parallel 2 > "build-stack1-$arm.log" 2>&1
 if ($LASTEXITCODE -ne 0) {exit $LASTEXITCODE}
}
exit 0
