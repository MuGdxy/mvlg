set bin=vkrefl.comp.spv
glslangValidator -V  vkrefl.comp -o vkrefl.comp.spv
spirv-reflect %bin% -y -v 1 > %bin%.yml