#define WLR_USE_UNSTABLE

#include <hyprland/src/helpers/Box.hpp>

#include <hyprland/src/render/Framebuffer.hpp>
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

class CWindowTransformer : public IWindowTransformer {
public:
  CWindowTransformer() {}

  virtual CFramebuffer *transform(CFramebuffer *in);
  virtual void preWindowRender(SRenderData *pRenderData, CFramebuffer *in,
                               struct wlr_surface *surface);

private:
  unsigned int shader, vertex, fragment;
};
std::vector<CWindowTransformer *> ptrs;

// Shader sources
const GLchar *vertexSource =
    "#version 330 core\n"
    "in vec2 position;"
    "in vec2 texcoord;"
    "out vec2 TexCoord;"
    "void main() {"
    "    TexCoord = texcoord;"
    "    gl_Position = vec4(position.x, position.y, 0.0, 1.0);"
    "}";

// const GLchar *fragmentSource = "#version 330 core\n"
//                                "in vec2 TexCoord;"
//                                "out vec4 FragColor;"
//                                "uniform sampler2D texture0;"
//                                "void main() {"
//                                "    vec4 color = texture(texture0,
//                                TexCoord);" "    color.r = 0.0;" // Remove red
//                                component "    FragColor = color;"
//                                "}";

const GLchar *fragmentSource =
    "#version 330 core\n"
    "in vec2 TexCoord;"
    "out vec4 FragColor;"
    "uniform sampler2D texture0;"
    "void main() {"
    "    vec3 color = texture(texture0, TexCoord).rgb;"
    "    color.r = 0.0;" // Remove red component
    "    FragColor = vec4(vec3(1.0 - color), 1.0);"
    "}";

// glAttachShader(prog, vertCompiled);
// glAttachShader(prog, fragCompiled);
// glLinkProgram(prog);

// glDetachShader(prog, vertCompiled);
// glDetachShader(prog, fragCompiled);
// glDeleteShader(vertCompiled);
// glDeleteShader(fragCompiled);

void CWindowTransformer::preWindowRender(SRenderData *pRenderData,
                                         CFramebuffer *in,
                                         struct wlr_surface *surface) {

  // oldPos = {(double)pRenderData->x, (double)pRenderData->y};
  CFramebuffer fb;

  if (!fb.isAllocated() || fb.m_vSize != in->m_vSize) {
    fb.release();
    fb.alloc(in->m_vSize.x, in->m_vSize.y);
  }

  const auto TEXTURE = wlr_surface_get_texture(pRenderData->surface);
  const CTexture &tex = TEXTURE;

  // 2. compile shaders
  vertex = glCreateShader(GL_VERTEX_SHADER);
  // glShaderSource(shader, 1, (const GLchar **)&shaderSource, nullptr);
  glShaderSource(vertex, 1, (const GLchar **)&vertexSource, NULL);
  glCompileShader(vertex);
  // checkCompileErrors(vertex, "VERTEX");
  // fragment Shader
  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, (const GLchar **)&fragmentSource, NULL);
  glCompileShader(fragment);
  // checkCompileErrors(fragment, "FRAGMENT");
  // shader Program
  shader = glCreateProgram();
  glAttachShader(shader, vertex);
  glAttachShader(shader, fragment);
  glLinkProgram(shader);
  // checkCompileErrors(ID, "PROGRAM");
  // delete the shaders as they're linked into our program now and no longer
  // necessary
  glDeleteShader(vertex);
  glDeleteShader(fragment);

  int wid = in->m_vSize.x;
  int he = in->m_vSize.y;
  // std::cout << pRenderData->w << wid << std::endl;
  // std::cout << pRenderData->h << he << std::endl;
  // int wid = (int)std::round(pRenderData->pWindow->m_vReportedSize.x);
  // int he = (int)std::round(pRenderData->pWindow->m_vReportedSize.x);

  // glViewport(0, 0, wid, he);

  // cover entire screen
  float quadVertices[] = {// vertex attributes for a quad that fills the entire
                          // screen in Normalized Device Coordinates.
                          // positions   // texCoords
                          -1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, -1.0f,
                          0.0f,  0.0f, 1.0f, -1.0f, 1.0f,  0.0f,

                          -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  -1.0f,
                          1.0f,  0.0f, 1.0f, 1.0f,  1.0f,  1.0f};
  unsigned int quadVAO, quadVBO;
  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));

  // apply shader
  glUseProgram(shader);
  glUniform1i(glGetUniformLocation(shader, "texture0"), 0);
  fb.bind();

  // GLuint FFrameBuffer;
  // setup FBO
  // glGenFramebuffers(1, &FFrameBuffer);
  // glBindFramebuffer(GL_FRAMEBUFFER, FFrameBuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         tex.m_iTexID, 0);
  glBindTexture(GL_TEXTURE_2D, tex.m_iTexID);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  // // glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // save texture to file (for debugging)
  GLubyte *pixels =
      new GLubyte[wid * he * 4]; // Assuming RGBA for 4 bytes per pixel
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  int a = rand();
  std::string str = "outout_" + std::to_string(a);
  stbi_write_png(str.c_str(), wid, he, 4, pixels, wid * 4);

  // TODO: do I need to copy the target back into tex.m_iTexID? (is already
  // there right?)
}

CFramebuffer *CWindowTransformer::transform(CFramebuffer *in) { return in; }

static void onNewWindow(void *self, std::any data) {
  // data is guaranteed
  auto *const PWINDOW = std::any_cast<CWindow *>(data); // data is guaranteed

  HyprlandAPI::addNotification(PHANDLE, "[window-transparency] start window!",
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);

  PWINDOW->m_vTransformers.push_back(std::make_unique<CWindowTransformer>());

  HyprlandAPI::addNotification(PHANDLE, "[window-transparency] end window",
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
  PHANDLE = handle;

  const std::string HASH = __hyprland_api_get_hash();

  if (HASH != GIT_COMMIT_HASH) {
    HyprlandAPI::addNotification(
        PHANDLE,
        "[hyprbars] Failure in initialization: Version mismatch (headers ver "
        "is not equal to running hyprland ver)",
        CColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error("[hb] Version mismatch");
  }

  HyprlandAPI::registerCallbackDynamic(
      PHANDLE, "openWindow",
      [&](void *self, SCallbackInfo &info, std::any data) {
        onNewWindow(self, data);
      });

  HyprlandAPI::addNotification(
      PHANDLE, "[window-transparency] Initialized successfully!",
      CColor{0.2, 1.0, 0.2, 1.0}, 5000);

  return {"window-transparency", "Apply shaders on each window", "M", "1.0"};
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