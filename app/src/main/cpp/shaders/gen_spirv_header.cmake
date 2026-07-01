# Reads the compiled SPIR-V files produced by glslc and emits a C header
# (ShaderSPIRV.h) embedding them as byte arrays. Run via:
#   cmake -Dspv_dir=<dir> -Dout=<header> -P gen_spirv_header.cmake

file(READ "${spv_dir}/shader.vert.spv" vert HEX)
file(READ "${spv_dir}/shader.frag.spv" frag HEX)

string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " vert_bytes "${vert}")
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " frag_bytes "${frag}")

file(WRITE "${out}"
"// AUTO-GENERATED from shaders/*.vert|frag by gen_spirv_header.cmake.\n"
"// Do not edit by hand; edit the GLSL sources and rebuild to regenerate.\n"
"#pragma once\n"
"#include <cstdint>\n"
"static const uint8_t kVertSpv[] = { ${vert_bytes} };\n"
"static const uint32_t kVertSpvSize = sizeof(kVertSpv);\n"
"static const uint8_t kFragSpv[] = { ${frag_bytes} };\n"
"static const uint32_t kFragSpvSize = sizeof(kFragSpv);\n")

