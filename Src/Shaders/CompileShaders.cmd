@echo off
rem Copyright (c) Microsoft Corporation. All rights reserved.
rem Licensed under the MIT License.

setlocal
set error=0

set FXCOPTS=/nologo /WX /Ges /Zi /Zpc /Qstrip_reflect /Qstrip_debug

if %1.==xbox. goto continuexbox
if %1.==. goto continuepc
echo usage: CompileShaders [xbox]
exit /b

:continuexbox
set XBOXOPTS=/D__XBOX_DISABLE_SHADER_NAME_EMPLACEMENT
if NOT %2.==noprecompile. goto skipnoprecompile
set XBOXOPTS=%XBOXOPTS% /D__XBOX_DISABLE_PRECOMPILE=1
:skipnoprecompile

set XBOXFXC="%XboxOneXDKLatest%\xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%XboxOneXDKLatest%xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%XboxOneXDKBuild%xdk\FXC\amd64\FXC.exe"
if exist %XBOXFXC% goto continue
set XBOXFXC="%DurangoXDK%xdk\FXC\amd64\FXC.exe"
if not exist %XBOXFXC% goto needxdk
goto continue

:continuepc

set PCFXC="%WindowsSdkBinPath%%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\%WindowsSDKVersion%\x86\fxc.exe"
if exist %PCFXC% goto continue
set PCFXC="%WindowsSdkDir%bin\x86\fxc.exe"
if exist %PCFXC% goto continue

set PCFXC=fxc.exe

:continue
@if not exist Compiled mkdir Compiled

call :CompileShader%1 BasicEffect vs VSBasic
call :CompileShader%1 BasicEffect vs VSBasicNoFog
call :CompileShader%1 BasicEffect vs VSBasicVc
call :CompileShader%1 BasicEffect vs VSBasicVcNoFog
call :CompileShader%1 BasicEffect vs VSBasicTx
call :CompileShader%1 BasicEffect vs VSBasicTxNoFog
call :CompileShader%1 BasicEffect vs VSBasicTxVc
call :CompileShader%1 BasicEffect vs VSBasicTxVcNoFog

call :CompileShader%1 BasicEffect ps PSBasic
call :CompileShader%1 BasicEffect ps PSBasicNoFog
call :CompileShader%1 BasicEffect ps PSBasicTx
call :CompileShader%1 BasicEffect ps PSBasicTxNoFog


call :CompileShaderSM4%1 PBREffect vs VSConstant
call :CompileShaderSM4%1 PBREffect vs VSConstantVelocity
call :CompileShaderSM4%1 PBREffect vs VSConstantBn
call :CompileShaderSM4%1 PBREffect vs VSConstantVelocityBn

call :CompileShaderSM4%1 PBREffect ps PSConstant
call :CompileShaderSM4%1 PBREffect ps PSTextured
call :CompileShaderSM4%1 PBREffect ps PSTexturedEmissive
call :CompileShaderSM4%1 PBREffect ps PSTexturedVelocity
call :CompileShaderSM4%1 PBREffect ps PSTexturedEmissiveVelocity

call :CompileShaderSM4%1 DebugEffect vs VSDebug
call :CompileShaderSM4%1 DebugEffect vs VSDebugBn
call :CompileShaderSM4%1 DebugEffect vs VSDebugVc
call :CompileShaderSM4%1 DebugEffect vs VSDebugVcBn

call :CompileShaderSM4%1 DebugEffect ps PSHemiAmbient
call :CompileShaderSM4%1 DebugEffect ps PSRGBNormals
call :CompileShaderSM4%1 DebugEffect ps PSRGBTangents
call :CompileShaderSM4%1 DebugEffect ps PSRGBBiTangents

call :CompileShaderSM4%1 PostProcess vs VSQuad

call :CompileShaderSM4%1 ToneMap vs VSQuad
call :CompileShaderSM4%1 ToneMap ps PSCopy
call :CompileShaderSM4%1 ToneMap ps PSHDR10

if NOT %1.==xbox. goto skipxboxonly

call :CompileShaderSM4xbox ToneMap ps PSHDR10_Saturate
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Reinhard
call :CompileShaderSM4xbox ToneMap ps PSHDR10_ACESFilmic
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Saturate_SRGB
call :CompileShaderSM4xbox ToneMap ps PSHDR10_Reinhard_SRGB
call :CompileShaderSM4xbox ToneMap ps PSHDR10_ACESFilmic_SRGB

:skipxboxonly

echo.

if %error% == 0 (
    echo Shaders compiled ok
) else (
    echo There were shader compilation errors!
)

endlocal
exit /b

:CompileShader
set fxc=%PCFXC% %1.fx %FXCOPTS% /T%2_4_0_level_9_1 /E%3 /FhCompiled\%1_%3.inc /FdCompiled\%1_%3.pdb /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderSM4
set fxc=%PCFXC% %1.fx %FXCOPTS% /T%2_4_0 /E%3 /FhCompiled\%1_%3.inc /FdCompiled\%1_%3.pdb /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderHLSL
set fxc=%PCFXC% %1.hlsl %FXCOPTS% /T%2_4_0_level_9_1 /E%3 /FhCompiled\%1_%3.inc /FdCompiled\%1_%3.pdb /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderxbox
:CompileShaderSM4xbox
set fxc=%XBOXFXC% %1.fx %FXCOPTS% /T%2_5_0 %XBOXOPTS% /E%3 /FhCompiled\XboxOne%1_%3.inc /FdCompiled\XboxOne%1_%3.pdb /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:CompileShaderHLSLxbox
set fxc=%XBOXFXC% %1.hlsl %FXCOPTS% /T%2_5_0 %XBOXOPTS% /E%3 /FhCompiled\XboxOne%1_%3.inc /FdCompiled\XboxOne%1_%3.pdb /Vn%1_%3
echo.
echo %fxc%
%fxc% || set error=1
exit /b

:needxdk
echo ERROR: CompileShaders xbox requires the Microsoft Xbox One XDK
echo        (try re-running from the XDK Command Prompt)