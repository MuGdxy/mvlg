set name=index.comp
glslc.exe -fshader-stage=compute %name% -o %name%.spv
spirv-reflect %name%.spv -y -v 1 > %name%.spv.yml
pause