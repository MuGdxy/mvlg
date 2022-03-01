glslc.exe -fshader-stage=compute multiply.comp -o multiply.spv
spirv-reflect multiply.spv -y -v 1 > multiply.spv.yml
pause