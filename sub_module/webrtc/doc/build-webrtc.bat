
@goto build-webrtc-win32
1.接口工程
连麦接口:MediaChat.
降噪接口:NoiseSuppression.
2.替换操作
目录src下buildtools,third_party以及depot_tools下python2.7是WIN依赖项。
目录src下的文件是基于webrtc代码的开发，这里提取出来方便移植。
将doc/develop-module/src目录内容差分合并到src中.
3.工程编译
将本脚本放在src目录下双击执行,然后复制echo"xxxxxx"中的命令执行就可以生成相应的all.sln工程文件。
然后用vs2015打开src/out/release_x86/all.sln直接编译就可以了。
4.注意事项
异常请检查：python2.7, depot_tools路径是否添加到系统环境变量PATH及wdk版本是否正确,
官方推荐WDK10.0.14393.795和10.0.15063.468,其他版本可能会有异常.
---BUILD-MAC---
gn gen out/debug-mac --ide=xcode --args='target_os="mac" target_cpu="x64" is_component_build=false'
---BUILD-MAC---
:build-webrtc-win32

set DEPOT_TOOLS_WIN_TOOLCHAIN=0 
set GYP_GENERATORS=msvs-ninja,ninja
set GYP_MSVS_VERSION=2015
set PATH=E:\work\webrtc\depot_tools;E:\work\webrtc\depot_tools\python276_bin;%PATH%
@echo ------------------------------------------------------------------------------
@echo Hi! input the following command for building( bellow src dir )!
@cd   E:\work\webrtc\webrtc-checkout\src
@echo gn gen out/Debug_x86 --ide=vs2015 --args="is_debug=true target_cpu=\"x86\""
@echo gn gen out/Release_x86 --ide=vs2015 --args="is_debug=false target_cpu=\"x86\""
@echo ------------------------------------------------------------------------------
@cmd
