#define WLR_USE_UNSTABLE

#include <hyprland/src/render/OpenGL.hpp>

#include <hyprland/src/helpers/Box.hpp>

#include <hyprland/src/render/Framebuffer.hpp>
#include <hyprland/src/render/shaders/Textures.hpp>
#include <string>
#include <unistd.h>

#include <GL/gl.h>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

inline HANDLE PHANDLE = nullptr;

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

inline const std::string myTEXVERTSRC = R"#(
uniform mat3 proj;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec2 v_texcoord;

void main() {
    gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);
    v_texcoord = texcoord;
})#";

inline const std::string myTEXFRAGSRCRGBAPASSTHRU = R"#(
precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

void main() {
    vec4 color = texture2D(tex, v_texcoord).rgba;
    color.r = 0.5;
    gl_FragColor = color;
})#";

std::string loadShader(std::string path) {
  if (path == "" || path == STRVAL_EMPTY)
    return "";

  std::ifstream infile(absolutePath(path, g_pConfigManager->getConfigDir()));

  if (!infile.good()) {
    g_pConfigManager->addParseError(
        "[window-shaders]: Screen shader parser: Screen shader path not found");
    return "";
  }

  std::string shader((std::istreambuf_iterator<char>(infile)),
                     (std::istreambuf_iterator<char>()));
  return shader;
}

class CWindowTransformer : public IWindowTransformer {
public:
  CWindowTransformer(const std::string &fragmentShader,
                     const std::string &vertexShader) {
    std::string fragShader = loadShader(fragmentShader);
    if (!fragShader.empty()) {
      this->fragmentShader = fragShader;
    }

    std::string vertShader = loadShader(vertexShader);
    if (!vertShader.empty()) {
      this->vertexShader = vertexShader;
    }
  }
  virtual CFramebuffer *transform(CFramebuffer *in);
  virtual void preWindowRender(SRenderData *pRenderData);

private:
  CFramebuffer fb;
  int width;
  int height;
  std::string fragmentShader = myTEXVERTSRC;
  std::string vertexShader = myTEXFRAGSRCRGBAPASSTHRU;
};

std::vector<CWindowTransformer *> ptrs;

GLfloat identityMatrix[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.0f, 0.0f, 1.0f};

GLfloat quadVert[] = {
    1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f, -1.0f, 1.0f,  0.0f, 1.0f,

    1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f};

// Derived class inheriting from Base
class CShaderEXT : public CShader {
public:
  GLint vbo = -1;
  GLuint quadVAO = -1;
  GLuint quadVBO = -1;
};

CShaderEXT shader;

void writeToFile(int width, int height) {
  GLubyte *pixels = new GLubyte[width * height * 4];
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  int a = rand();
  std::string str = "/home/aaahh/tmp/outout_" + std::to_string(a);
  stbi_write_png(str.c_str(), width, height, 4, pixels, width * 4);
}

// std::string convertToString(const char *const *const &strPtrPtr)
// {
//   const char *const strPtr = *strPtrPtr;
//   return std::string(strPtr);
// }

std::string convertToString(std::string strPtrPtr) {
  // // if ((std::string)strPtrPtr.length())
  std::cerr << "Error: eee is null." << std::endl;
  HyprlandAPI::addNotification(PHANDLE, "[window-shaders]" + strPtrPtr,
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);
  // if (strPtrPtr == nullptr)
  // {
  //   std::cerr << "Error: strPtrPtr is null." << std::endl;
  //   return ""; // Return an empty string or handle the error as appropriate
  // }
  // std::cerr << "Error: eee3 is null." << std::endl;
  // std::string yer =  strPtrPtr; // Potential issue here
  // std::cerr << yer << "ww"<< std::endl;
  return strPtrPtr;
}

void CWindowTransformer::preWindowRender(SRenderData *pRenderData) {
  const auto TEXTURE = wlr_surface_get_texture(pRenderData->surface);
  const CTexture &windowTexture = TEXTURE;

  width = windowTexture.m_vSize.x;
  height = windowTexture.m_vSize.y;

  if (!fb.isAllocated() || fb.m_vSize != windowTexture.m_vSize) {
    fb.release();
    fb.alloc(width, height);
  }

  CShader *passThroughShader =
      &g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shPASSTHRURGBA;
  if (!shader.program) {
    shader.program = g_pHyprOpenGL->createProgram(
        myTEXVERTSRC, myTEXFRAGSRCRGBAPASSTHRU, false);
    shader.proj = glGetUniformLocation(shader.program, "proj");
    shader.tex = glGetUniformLocation(shader.program, "tex");
    shader.texAttrib = glGetAttribLocation(shader.program, "texcoord");
    shader.posAttrib = glGetAttribLocation(shader.program, "pos");

    glGenVertexArrays(1, &shader.quadVAO);
    glGenBuffers(1, &shader.quadVBO);
    glBindVertexArray(shader.quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shader.quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVert), &quadVert, GL_STATIC_DRAW);
    glVertexAttribPointer(shader.posAttrib, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), 0);
    glVertexAttribPointer(shader.texAttrib, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(shader.posAttrib);
    glEnableVertexAttribArray(shader.texAttrib);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  glViewport(0, 0, width, height);
  glBindVertexArray(shader.quadVAO);
  glBindFramebuffer(GL_FRAMEBUFFER, fb.m_iFb);

  // ping: render with user's shader
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         fb.m_cTex.m_iTexID, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, windowTexture.m_iTexID);

  glUseProgram(shader.program);
  glUniformMatrix3fv(shader.proj, 1, GL_FALSE, identityMatrix);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  // writeToFile(width, height);

  // pong: write back to window texture
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         windowTexture.m_iTexID, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, fb.m_cTex.m_iTexID);

  glUseProgram(passThroughShader->program);
  glUniformMatrix3fv(passThroughShader->proj, 1, GL_FALSE, identityMatrix);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  // unbind buffer and array
  glBindVertexArray(0);
}

CFramebuffer *CWindowTransformer::transform(CFramebuffer *in) { return in; }

static void onNewWindow(void *self, std::any data) {
  // data is guaranteed
  auto *const PWINDOW = std::any_cast<CWindow *>(data);
  static auto *const PCLASS =
      (Hyprlang::STRING const *)HyprlandAPI::getConfigValue(
          PHANDLE, "plugin:window-shaders:class")
          ->getDataStaticPtr();
  if (((std::string)*PCLASS).length() ==
          0 || // select all windows if not specified
      (g_pCompositor->m_pLastWindow &&
       g_pCompositor->m_pLastWindow->m_szInitialClass == *PCLASS &&
       g_pCompositor->m_pLastMonitor)) {
    static auto *const PFRAGMENTSHADER =
        (Hyprlang::STRING const *)HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:window-shaders:fragment-shader")
            ->getDataStaticPtr();
    static auto *const PVERTEXSHADER =
        (Hyprlang::STRING const *)HyprlandAPI::getConfigValue(
            PHANDLE, "plugin:window-shaders:vertex-shader")
            ->getDataStaticPtr();

    std::string fragmentShader(*PFRAGMENTSHADER);
    std::string vertexShader(*PVERTEXSHADER);

    // TODO pass PFRAGMENTSHADER and PVERTEXSHADER
    PWINDOW->m_vTransformers.push_back(
        std::make_unique<CWindowTransformer>(fragmentShader, vertexShader));
  }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
  PHANDLE = handle;

  const std::string HASH = __hyprland_api_get_hash();

  if (HASH != GIT_COMMIT_HASH) {
    HyprlandAPI::addNotification(PHANDLE,
                                 "[window-shaders] Failure in initialization: "
                                 "Version mismatch (headers ver "
                                 "is not equal to running hyprland ver)",
                                 CColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error("[window-shaders] Version mismatch");
  }

  HyprlandAPI::addConfigValue(PHANDLE, "plugin:window-shaders:class",
                              Hyprlang::STRING{""});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:window-shaders:fragment-shader",
                              Hyprlang::STRING{""});
  HyprlandAPI::addConfigValue(PHANDLE, "plugin:window-shaders:vertex-shader",
                              Hyprlang::STRING{""});

  HyprlandAPI::registerCallbackDynamic(
      PHANDLE, "openWindow",
      [&](void *self, SCallbackInfo &info, std::any data) {
        onNewWindow(self, data);
      });

  HyprlandAPI::addNotification(PHANDLE,
                               "[window-shaders] Initialized successfully!",
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);

  return {"window-shaders", "Apply shaders on each window", "M", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
  for (auto &w : g_pCompositor->m_vWindows) {
    std::erase_if(
        w->m_vTransformers,
        [&](const std::unique_ptr<IWindowTransformer> &other) {
          return std::find_if(ptrs.begin(), ptrs.end(), [&](const auto &p) {
                   return static_cast<IWindowTransformer *>(p) == other.get();
                 }) != ptrs.end();
        });
  }
}