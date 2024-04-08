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

class CWindowTransformer : public IWindowTransformer
{
public:
  virtual CFramebuffer *transform(CFramebuffer *in);
  virtual void preWindowRender(SRenderData *pRenderData);

private:
  CFramebuffer fb;
};

std::vector<CWindowTransformer *> ptrs;

inline const std::string myTEXVERTSRC = R"#(
uniform mat3 proj;
attribute vec2 pos;
attribute vec2 texcoord;
varying vec2 v_texcoord;

void main() {
    gl_Position = vec4(vec3(pos, 1.0), 1.0);
    v_texcoord = texcoord;
})#";

inline const std::string myTEXFRAGSRCRGBAPASSTHRU = R"#(
precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;

void main() {
vec3 color = texture2D(tex, v_texcoord).rgb;
color.r = 0.5;
gl_FragColor = vec4(color, 1.0);
})#";

void writeToFile(int wid, int he){
  GLubyte *pixels = new GLubyte[wid * he * 4];
  glReadPixels(0, 0, wid, he, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  int a = rand();
  std::string str = "/home/aaahh/tmp/outout_" + std::to_string(a);
  stbi_write_png(str.c_str(), wid, he, 4, pixels, wid * 4);
}

void CWindowTransformer::preWindowRender(SRenderData *pRenderData)
{
  const auto TEXTURE = wlr_surface_get_texture(pRenderData->surface);
  const CTexture &tex = TEXTURE;
  int wid = tex.m_vSize.x;
  int he = tex.m_vSize.y;

  // if (!fb.isAllocated() || fb.m_vSize.x != pRenderData->w ||
  //     fb.m_vSize.y != pRenderData->h)
  // {
  //   fb.release();
  //   fb.alloc(pRenderData->w, pRenderData->h);
  // }
  // fb.bind();
  // g_pHyprOpenGL->clear(CColor(0, 0, 0, 0));

  CShader *shader = &g_pHyprOpenGL->m_sWindowShader;
  if (!shader->program)
  {
    shader->program = g_pHyprOpenGL->createProgram(
        myTEXVERTSRC, myTEXFRAGSRCRGBAPASSTHRU, true);
    shader->proj = glGetUniformLocation(shader->program, "proj");
    shader->tex = glGetUniformLocation(shader->program, "tex");
    shader->texAttrib = glGetAttribLocation(shader->program, "texcoord");
    shader->posAttrib = glGetAttribLocation(shader->program, "pos");
    std::cout << "GEN SHADER " << std::endl;
  }
  
  glViewport(0, 0, wid, he);

  // unsigned int quadVAO, quadVBO;
  // glGenVertexArrays(1, &quadVAO);
  // glGenBuffers(1, &quadVBO);
  // glBindVertexArray(quadVAO);
  // glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  // glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
  //              GL_STATIC_DRAW);

  GLfloat vertices1[] = {// vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
                         // positions   // texCoords
                         1.0f, -1.0f,
                         1.0f, 1.0f,
                         -1.0f, 1.0f,

                         1.0f, -1.0f,
                         -1.0f, 1.0f,
                         -1.0f, -1.0f};

  GLfloat vertices2[] = {// vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
                         // positions   // texCoords
                         0.0f, 1.0f,
                         0.0f, 0.0f,
                         1.0f, 0.0f,

                         0.0f, 1.0f,
                         1.0f, 0.0f,
                         1.0f, 1.0f};

  glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0 * sizeof(float),
                        vertices1);
  glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0 * sizeof(float),
                        vertices2);
  glEnableVertexAttribArray(shader->posAttrib);
  glEnableVertexAttribArray(shader->texAttrib);

  GLuint frameBuffer;
  glGenFramebuffers(1, &frameBuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
  glViewport(0, 0, wid, he);

  GLuint texColorBuffer;
  glGenTextures(1, &texColorBuffer);
  glBindTexture(GL_TEXTURE_2D, texColorBuffer);

  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB, wid, he, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

  GLuint frameBuffer2;
  glGenFramebuffers(1, &frameBuffer2);
  glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer2);
  glViewport(0, 0, wid, he);

  GLuint texColorBuffer2;
  glGenTextures(1, &texColorBuffer2);
  glBindTexture(GL_TEXTURE_2D, texColorBuffer2);

  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB, wid, he, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.m_iTexID, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
  glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex.m_iTexID);
  glUseProgram(shader->program);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "PP" << status << std::endl;
  }
  else
  {
    std::cout << "looks good" << std::endl;
  }
  glViewport(0, 0, wid, he);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer2);
  glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texColorBuffer);
  glUseProgram(shader->program);
  status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE)
  {
    std::cout << "PP2" << status << std::endl;
  }
  else
  {
    std::cout << "looks good2" << std::endl;
  }
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

CFramebuffer *CWindowTransformer::transform(CFramebuffer *in) { return in; }

static void onNewWindow(void *self, std::any data)
{
  // data is guaranteed
  auto *const PWINDOW = std::any_cast<CWindow *>(data); // data is guaranteed

  HyprlandAPI::addNotification(PHANDLE, "[window-shaders] start window!",
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);

  PWINDOW->m_vTransformers.push_back(std::make_unique<CWindowTransformer>());

  HyprlandAPI::addNotification(PHANDLE, "[window-shaders] end window",
                               CColor{0.2, 1.0, 0.2, 1.0}, 5000);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
  PHANDLE = handle;

  const std::string HASH = __hyprland_api_get_hash();

  if (HASH != GIT_COMMIT_HASH)
  {
    HyprlandAPI::addNotification(
        PHANDLE,
        "[window-shaders] Failure in initialization: Version mismatch (headers ver "
        "is not equal to running hyprland ver)",
        CColor{1.0, 0.2, 0.2, 1.0}, 5000);
    throw std::runtime_error("[hb] Version mismatch");
  }

  HyprlandAPI::registerCallbackDynamic(
      PHANDLE, "openWindow",
      [&](void *self, SCallbackInfo &info, std::any data)
      {
        onNewWindow(self, data);
      });

  HyprlandAPI::addNotification(
      PHANDLE, "[window-shaders] Initialized successfully!",
      CColor{0.2, 1.0, 0.2, 1.0}, 5000);

  return {"window-shaders", "Apply shaders on each window", "M", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT()
{
  for (auto &w : g_pCompositor->m_vWindows)
  {
    std::erase_if(
        w->m_vTransformers,
        [&](const std::unique_ptr<IWindowTransformer> &other)
        {
          return std::find_if(ptrs.begin(), ptrs.end(), [&](const auto &p)
                              { return static_cast<IWindowTransformer *>(p) == other.get(); }) != ptrs.end();
        });
  }
}