set name=frag.shader
glslc.exe -fshader-stage=frag %name% -o %name%.spv
spirv-reflect %name%.spv -y -v 1 > %name%.spv.yml
pause