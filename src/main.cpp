#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <GLES3/gl32.h>

#include "image_utils.h"

static void runFSR(struct FSRConstants fsrData, uint32_t fsrProgramEASU, uint32_t fsrProgramRCAS, uint32_t fsrData_vbo, uint32_t inputImage, uint32_t outputImage) {
    uint32_t displayWidth = fsrData.output.width;
    uint32_t displayHeight = fsrData.output.height;

    static const int threadGroupWorkRegionDim = 16;
    int dispatchX = (displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
    int dispatchY = (displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;


    // binding point constants in the shaders
    const int inFSRDataPos = 0;
    const int inFSRInputTexture = 1;
    const int inFSROutputTexture = 2;

    { // run FSR EASU
        glUseProgram(fsrProgramEASU);

        // connect the input uniform data
        glBindBufferBase(GL_UNIFORM_BUFFER, inFSRDataPos, fsrData_vbo);

        // bind the input image to a texture unit
        glActiveTexture(GL_TEXTURE0 + inFSRInputTexture);
        glBindTexture(GL_TEXTURE_2D, inputImage);

        // connect the output image
        glBindImageTexture(inFSROutputTexture, outputImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(dispatchX, dispatchY, 1);
        glFinish();
    }

    {
        // FSR RCAS
        // connect the input uniform data
        glBindBufferBase(GL_UNIFORM_BUFFER, inFSRDataPos, fsrData_vbo);

        // connect the previous image's output as input
        glActiveTexture(GL_TEXTURE0 + inFSRInputTexture);
        glBindTexture(GL_TEXTURE_2D, outputImage);

        // connect the output image which is the same as the input image
        glBindImageTexture(inFSROutputTexture, outputImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glUseProgram(fsrProgramRCAS);
        glDispatchCompute(dispatchX, dispatchY, 1);
        glFinish();
    }
}

static void runBilinear(struct FSRConstants fsrData, uint32_t bilinearProgram, int32_t fsrData_vbo, uint32_t inputImage, uint32_t outputImage) {
    uint32_t displayWidth = fsrData.output.width;
    uint32_t displayHeight = fsrData.output.height;

    static const int threadGroupWorkRegionDim = 16;
    int dispatchX = (displayWidth + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
    int dispatchY = (displayHeight + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;


    // binding point constants in the shaders
    const int inFSRDataPos = 0;
    const int inFSRInputTexture = 1;
    const int inFSROutputTexture = 2;

    { // run FSR EASU
        glUseProgram(bilinearProgram);

        // connect the input uniform data
        glBindBufferBase(GL_UNIFORM_BUFFER, inFSRDataPos, fsrData_vbo);

        // bind the input image to a texture unit
        glActiveTexture(GL_TEXTURE0 + inFSRInputTexture);
        glBindTexture(GL_TEXTURE_2D, inputImage);

        // connect the output image
        glBindImageTexture(inFSROutputTexture, outputImage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute(dispatchX, dispatchY, 1);
        glFinish();
    }
}

uint32_t createOutputImage(struct FSRConstants fsrData) {
    uint32_t outputImage = 0;
    glGenTextures(1, &outputImage);
    glBindTexture(GL_TEXTURE_2D, outputImage);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, fsrData.output.width, fsrData.output.height);
    glBindTexture(GL_TEXTURE_2D, 0);

    return outputImage;
}

static void glfw_error_callback(int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void on_gl_error(GLenum source, GLenum type, GLuint id, GLenum severity,
                          GLsizei length, const GLchar* message, const void *userParam) {

    printf("-> %s\n", message);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <image>\n", argv[0]);
        return -1;
    }

    const char* input_image = argv[1];

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, 1);

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1024, 600, "GLES FSR", NULL, NULL);
    if (window == NULL) {
        return 1;
    }

    glfwMakeContextCurrent(window);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(on_gl_error, NULL);

    glfwSwapInterval(1);

    // GUI options:
    bool useFSR = true;
    float zoom = 1.0f;
    float moveX = 0.0f;
    float moveY = 1.0f;
    float resMultiplier = 4.0f;
    float rcasAtt = 0.25f;


    struct FSRConstants fsrData = {};

    uint32_t inputTexture = 0;
    bool ret = LoadTextureFromFile(input_image, &inputTexture, &fsrData.input.width, &fsrData.input.height);
    IM_ASSERT(ret);

    fsrData.output = { fsrData.input.width * resMultiplier, fsrData.input.height * resMultiplier };

    prepareFSR(&fsrData, rcasAtt);

    const std::string baseDir = "src/";

    uint32_t fsrProgramEASU = createFSRComputeProgramEAUS(baseDir);
    uint32_t fsrProgramRCAS = createFSRComputeProgramRCAS(baseDir);
    uint32_t bilinearProgram = createBilinearComputeProgram(baseDir);

    uint32_t outputImage = createOutputImage(fsrData);


    // upload the FSR constants, this contains the EASU and RCAS constants in a single uniform
    // TODO destroy the buffer
    unsigned int fsrData_vbo;
    {
        glGenBuffers(1, &fsrData_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, fsrData_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(fsrData), &fsrData, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    runFSR(fsrData, fsrProgramEASU, fsrProgramRCAS, fsrData_vbo, inputTexture, outputImage);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color{0.1f, 0.1f, 0.1f, 1.0f};


    while (!glfwWindowShouldClose(window))
    {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Render app
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            bool changed = false;

            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::Begin("FSR RCAS config");

            changed |= ImGui::Checkbox("Enable FSR", &useFSR);
            changed |= ImGui::SliderFloat("Resolution Multiplier", &resMultiplier, 0.0001, 10.0f);
            changed |= ImGui::SliderFloat("rcasAttenuation", &rcasAtt, 0.0f, 2.0f);

            if (changed) {
                Extent oldOutput = fsrData.output;

                fsrData.output = { fsrData.input.width * resMultiplier, fsrData.input.height * resMultiplier };

                if (oldOutput.width != fsrData.output.width) {
                    glDeleteTextures(1, &outputImage);
                    outputImage = createOutputImage(fsrData);
                    printf("Recreated output image\n");
                }

                if (!useFSR) {
                    printf("Running Bilinear Program\n");
                    runBilinear(fsrData, bilinearProgram, fsrData_vbo, inputTexture, outputImage);
                } else {
                    prepareFSR(&fsrData, rcasAtt);

                    glBindBuffer(GL_ARRAY_BUFFER, fsrData_vbo);
                    glBufferData(GL_ARRAY_BUFFER, sizeof(fsrData), &fsrData, GL_DYNAMIC_DRAW);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);

                    printf("Running FSR\n");
                    runFSR(fsrData, fsrProgramEASU, fsrProgramRCAS, fsrData_vbo, inputTexture, outputImage);
                }
            }

            ImGui::SliderFloat("Zoom", &zoom, 0.000001f, 2.0f);
            ImGui::SliderFloat("Move X", &moveX, 0.0, 1.0f);
            ImGui::SliderFloat("Move Y", &moveY, 0.0, 1.0f);


            // Edit 3 floats representing a color
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            if (ImGui::Button("Exit")) {
                break;
            }

            ImGui::End();
        }

        ImVec2 displaySize = ImVec2(fsrData.output.width * zoom, fsrData.output.height * zoom);
        ImVec2 viewPosStart = ImVec2(moveX, moveX);
        ImVec2 viewPosEnd = ImVec2(moveY, moveY);

        ImGui::SetNextWindowPos(ImVec2(20, 150), ImGuiCond_FirstUseEver);
        ImGui::Begin("INPUT Image");
        ImGui::Text("pointer = %p", inputTexture);
        ImGui::Text("size = %d x %d", fsrData.input.width, fsrData.input.height);
        ImGui::Image((void*)(intptr_t)inputTexture, displaySize, viewPosStart, viewPosEnd);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(40, 200), ImGuiCond_FirstUseEver);
        ImGui::Begin("OUTPUT Image");
        ImGui::Text("pointer = %p", outputImage);
        ImGui::Text("size = %d x %d", fsrData.output.width, fsrData.output.height);
        ImGui::Image((void*)(intptr_t)outputImage, displaySize, viewPosStart, viewPosEnd);
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
