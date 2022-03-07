set name=vert.shader
glslc.exe -fshader-stage=vert %name% -o %name%.spv
spirv-reflect %name%.spv -y -v 1 > %name%.spv.yml
pause