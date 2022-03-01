glslangValidator -V -S vert triangle.shader -o triangle.vert.spv -DMU_VERTEX_SHADER
glslangValidator -V -S frag triangle.shader -o triangle.frag.spv -DMU_FRAGMENT_SHADER
spirv-reflect triangle.vert.spv -y -v 1 > triangle.vert.spv.yml
spirv-reflect triangle.frag.spv -y -v 1 > triangle.frag.spv.yml
spirv-reflect triangle.vert.spv
spirv-reflect triangle.frag.spv
pause