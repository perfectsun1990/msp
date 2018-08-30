<#
文件：webrtc-compile.ps1
用途：用于webrtc编译生成脚本
创建：2017-12-07
修改：2017-12-07
#>

<#---------------------------------------------------------------------#>
$script:DEBUG=1
$script:CLEAN=0
$script:ARCHI="x86"
$script:ARRAY="media_chat","NoiseSuppression"
$script:BUILD_SRC_PATH="E:\work\webrtc\webrtc-checkout\src\out"
$script:BUILD_DST_PATH="E:\work\client\5.0.0"
<#---------------------------------------------------------------------#>

function webrtc_compile()
{
    begin
    {        
        if($DEBUG){ $MODE="Debug" }else { $MODE="Release" }  
        $dirs = "$BUILD_DST_PATH\CCLive\lib","$BUILD_DST_PATH\plugins\win-dshow\lib","$BUILD_DST_PATH\CCLive\deps",`
                "$BUILD_DST_PATH\build\rundir\$MODE\bin\32bit","$BUILD_SRC_PATH\$MODE`_$ARCHI"
        foreach( $item in $dirs )
        {
             if( !(Test-Path $item) ){ echo "@err:$item is not find";return }
        }
    }
  
    process
    {
        foreach( $item in $ARRAY )
        {      
            echo "-----------------------------------------------------------------------------------------" 
            echo "@building [$item]"
            <# 编译生成 #>
            if($CLEAN) { ninja -C $BUILD_SRC_PATH\$MODE`_$ARCHI -t clean $item }
            ninja -C $BUILD_SRC_PATH\$MODE`_$ARCHI $item
            if(!$?){ echo "@err: $($error[0])";return }
            <# 检测拷贝 #>
            if($item -eq $ARRAY[0]) {
            cp $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll.lib $BUILD_DST_PATH\CCLive\lib\$item.lib }
            if($item -eq $ARRAY[1]) {
            cp $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll.lib $BUILD_DST_PATH\plugins\win-dshow\lib\$item.lib }
            if(!$?){ echo "@err: $($error[0])"; continue }
            cp $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll $BUILD_DST_PATH\CCLive\deps\$item.dll            
            cp $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll $BUILD_DST_PATH\build\rundir\$MODE\bin\32bit\$item.dll
            cp $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll.pdb $BUILD_DST_PATH\CCLive\deps\$item.dll.pdb
            if(!$?){ echo "@err: $($error[0])"; continue }
            echo "@copy $BUILD_SRC_PATH\$MODE`_$ARCHI\$item.dll && $item.dll.lib && $item.dll.pdb success! "
            echo "-----------------------------------------------------------------------------------------"     
        }  
    }

    end
    {
        if($?) {echo "@Note:you should first execute build-webrtc.bat to general ninja files!"}
    }
}
webrtc_compile