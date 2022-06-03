
#include <GLES3/gl32.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "image_utils.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_fsr1.h"

#include <memory>
#include <sstream>

// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, uint32_t* out_width, uint32_t* out_height)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    GLuint imm_image_texture;
    glGenTextures(1, &imm_image_texture);
    glBindTexture(GL_TEXTURE_2D, imm_image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, image_width, image_height);

    glCopyImageSubData(image_texture, GL_TEXTURE_2D, 0, 0, 0, 0, imm_image_texture, GL_TEXTURE_2D, 0, 0, 0, 0, image_width, image_height, 1);

    glDeleteTextures(1, &image_texture);
    glFinish();
    *out_texture = imm_image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

void prepareFSR(FSRConstants* fsrData, float rcasAttenuation)
{
    FsrEasuCon(fsrData->const0, fsrData->const1, fsrData->const2, fsrData->const3,
               fsrData->input.width, fsrData->input.height, // frame render resolution
               fsrData->input.width, fsrData->input.height, // input container resolution
               fsrData->output.width, fsrData->output.height); // upsacled resolution

    printf("EASU:\n");
    printf("Const0: %d %d %d %d\n", fsrData->const0[0], fsrData->const0[1], fsrData->const0[2], fsrData->const0[3]);
    printf("Const1: %d %d %d %d\n", fsrData->const1[0], fsrData->const1[1], fsrData->const1[2], fsrData->const1[3]);
    printf("Const2: %d %d %d %d\n", fsrData->const2[0], fsrData->const2[1], fsrData->const2[2], fsrData->const2[3]);
    printf("Const3: %d %d %d %d\n", fsrData->const3[0], fsrData->const3[1], fsrData->const3[2], fsrData->const3[3]);

    printf("Input: %dx%d\n", fsrData->input.width, fsrData->input.height);
    printf("Output: %dx%d\n", fsrData->output.width, fsrData->output.height);

    memcpy(fsrData->const0RCAS, fsrData->const0, sizeof(fsrData->const0));

    FsrRcasCon(fsrData->const0RCAS, rcasAttenuation);
    printf("RCAS: rcasAttenuation = %.3f\n", rcasAttenuation);
    printf("Const0: %d %d %d %d\n", fsrData->const0RCAS[0], fsrData->const0RCAS[1], fsrData->const0RCAS[2], fsrData->const0RCAS[3]);
}



static std::unique_ptr<uint8_t> readFile(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Unable to open: %s\n", filename);
        return 0;
    }

    fseek(fp, 0L, SEEK_END);
    size_t fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    std::unique_ptr<uint8_t> buffer(new uint8_t[fileSize + 1]);
    size_t readSize = fread(buffer.get(), 1, fileSize, fp);

    buffer.get()[readSize] = 0;

    return buffer;
}

static uint32_t compileProgram(const std::string& source) {
    const char* src = source.c_str();

    // C.1. Create the Compute shader
    unsigned int compute_shader;
    {
        compute_shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute_shader, 1, &src, NULL);
        glCompileShader(compute_shader);
        int success;
        glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info[512];
            glGetShaderInfoLog(compute_shader, 512, NULL, info);
            printf("Compute shader error:\n%s\n", info);
            return -3;
        }
    }

    // C.2. Create the Compute program
    unsigned int compute_program;
    {
        compute_program = glCreateProgram();
        glAttachShader(compute_program, compute_shader);
        glLinkProgram(compute_program);

        int success;
        glGetProgramiv(compute_program, GL_LINK_STATUS, &success);
        if(!success) {
            char info[512];
            glGetProgramInfoLog(compute_program, 512, NULL, info);
            printf("Compute Program error:\n%s\n", info);
            return -3;
        }
    }

    return compute_program;
}

static std::string buildShader(const std::vector<std::string>& headers, const std::vector<std::string>& filenames, const std::map<std::string, std::string>& defines)
{
    std::stringstream out;
    for (const std::string& header : headers) {
        out << header << std::endl;
    }

    out << "/* DEFINES */" << std::endl;
    for (const auto& item : defines) {
        out << "#define " << item.first << " " << item.second << std::endl;
    }
    out << "/*  */" << std::endl;

    for (const std::string& filename : filenames) {
        out << "/* Input file: " << filename << " */"  << std::endl;

        std::unique_ptr<uint8_t> data = readFile(filename.c_str());

        out << (const char*)data.get() << std::endl;
    }

    return out.str();
}

uint32_t createFSRComputeProgramEAUS(const std::string& baseDir) {
    std::map<std::string, std::string> defines = {
        { "A_GPU", "1" },
        { "A_GLSL", "1" },
        { "SAMPLE_SLOW_FALLBACK", "1" },
        { "SAMPLE_EASU", "1" },
        { "FSR_EASU_F", "1" },

        { "SAMPLE_RCAS", "0" },
        { "SAMPLE_BILINEAR", "0" },
    };
    std::vector<std::string> files = {
        baseDir + "ffx_a.h",
        baseDir + "ffx_fsr1.h",
        baseDir + "fsr_easu.compute.base.glsl"
    };
    std::vector<std::string> header = {
        "#version 320 es",
        //"#extension GL_KHR_vulkan_glsl : enable",
        //"#extension GL_ARB_separate_shader_objects : enable",
        //"#extension GL_ARB_shading_language_420pack : enable",
        //"#extension GL_GOOGLE_include_directive : enable",
    };

    std::string shader = buildShader(header, files, defines);


    return compileProgram(shader);
}

uint32_t createFSRComputeProgramRCAS(const std::string& baseDir) {
    std::map<std::string, std::string> defines = {
        { "A_GPU", "1" },
        { "A_GLSL", "1" },
        { "SAMPLE_SLOW_FALLBACK", "1" },
        { "SAMPLE_RCAS", "1" },
        { "FSR_RCAS_F", "1" },

        { "SAMPLE_EASU", "0" },
        { "SAMPLE_BILINEAR", "0" },
    };
    std::vector<std::string> files = {
        baseDir + "ffx_a.h",
        baseDir + "ffx_fsr1.h",
        baseDir + "fsr_easu.compute.base.glsl"
    };
    std::vector<std::string> header = {
        "#version 320 es",
        //"#extension GL_KHR_vulkan_glsl : enable",
        //"#extension GL_ARB_separate_shader_objects : enable",
        //"#extension GL_ARB_shading_language_420pack : enable",
        //"#extension GL_GOOGLE_include_directive : enable",
    };

    std::string shader = buildShader(header, files, defines);

    return compileProgram(shader);
}

uint32_t createBilinearComputeProgram(const std::string& baseDir) {

    std::map<std::string, std::string> defines = {
        { "A_GPU", "1" },
        { "A_GLSL", "1" },
        { "SAMPLE_BILINEAR", "1"},
        { "SAMPLE_SLOW_FALLBACK", "1" },

        { "SAMPLE_RCAS", "0" },
        { "FSR_RCAS_F", "0" },
        { "SAMPLE_EASU", "0" },
    };
    std::vector<std::string> files = {
        baseDir + "ffx_a.h",
//        baseDir + "ffx_fsr1.h",
        baseDir + "fsr_easu.compute.base.glsl"
    };
    std::vector<std::string> header = {
        "#version 320 es",
        //"#extension GL_KHR_vulkan_glsl : enable",
        //"#extension GL_ARB_separate_shader_objects : enable",
        //"#extension GL_ARB_shading_language_420pack : enable",
        //"#extension GL_GOOGLE_include_directive : enable",
    };

    std::string shader = buildShader(header, files, defines);

    return compileProgram(shader);
}
