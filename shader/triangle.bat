set name=triangle.shader
glslc.exe -fshader-stage=vert -DMU_VERTEX_SHADER %name% -o %name%.vert.spv
spirv-reflect %name%.vert.spv -y -v 1 > %name%.vert.spv.yml
glslc.exe -fshader-stage=frag -DMU_FRAGMENT_SHADER %name% -o %name%.frag.spv
spirv-reflect %name%.frag.spv -y -v 1 > %name%.frag.spv.yml
pause