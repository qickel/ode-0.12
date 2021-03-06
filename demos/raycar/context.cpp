
#include "rc_context.h"
#include "rc_bitmap.h"
#include "rc_asset.h"
#include "rc_model.h"

#include <GL/glew.h>

#include <stdint.h>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <map>
#include <assert.h>

void glAssertError_(char const *file, int line)
{
    uint32_t err = glGetError();
    if (err != GL_NO_ERROR)
    {
        char buf[200];
        _snprintf_s(buf, 200, "%s:%d:\nGraphics error: 0x%x", file, line, err);
        throw std::runtime_error(std::string(buf));
    }
}


static GLContext ctx;
static std::map<std::string, GLuint> bitmapTextures;

static GLuint defaultAmbientMap;
static GLuint defaultDiffuseMap;
static GLuint defaultSpecularMap;
static GLuint defaultEmissiveMap;
static GLuint defaultOpacityMap;
static GLuint defaultCustomMap;

static GLuint gProgram;
static GLuint gCustomProgram;

static GLuint uniformAmbientMap;
static GLuint uniformDiffuseMap;
static GLuint uniformSpecularMap;
static GLuint uniformEmissiveMap;
static GLuint uniformOpacityMap;
static GLuint customMap;


static GLuint loadTexture(std::string const &name, GLuint def)
{
    if (name.empty())
    {
        return def;
    }
    std::map<std::string, GLuint>::iterator ptr(bitmapTextures.find(name));
    if (ptr != bitmapTextures.end())
    {
        return (*ptr).second;
    }
    Bitmap *bm = Asset::bitmap(name);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glAssertError();
    GLuint fmt = GL_LUMINANCE;
    switch (bm->bytesPerPixel())
    {
    case 1: fmt = GL_LUMINANCE; break;
    case 3: fmt = GL_BGR; break;
    case 4: fmt = GL_BGRA; break;
    default: assert(!"unknown bits per pixel"); break;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bm->width(), bm->height(), 0, fmt, GL_UNSIGNED_BYTE, bm->bits());
    glAssertError();
    glGenerateMipmap(GL_TEXTURE_2D);
    glAssertError();
    bitmapTextures[name] = tex;
    return tex;
}

class GLMaterial : public BuiltMaterial
{
public:
    GLMaterial(GLContext *ctx) :
        refCount_(1)
    {
        isTransparent_ = false;
    }
    ~GLMaterial()
    {
    }
    void configure(Material const &mtl);
    void apply();
    void release();

    int refCount_;

    float shininess;
    Rgba ambient;
    Rgba diffuse;
    Rgba specular;
    Rgba emissive;
    GLuint ambientMap;
    GLuint diffuseMap;
    GLuint specularMap;
    GLuint emissiveMap;
    GLuint opacityMap;
};

void GLMaterial::configure(Material const &mtl)
{
    shininess = mtl.specPower;
    ambient = mtl.colors[mk_ambient];
    diffuse = mtl.colors[mk_diffuse];
    specular = mtl.colors[mk_specular];
    emissive = mtl.colors[mk_emissive];
    ambientMap = loadTexture(mtl.maps[mk_ambient].name, defaultAmbientMap);
    diffuseMap = loadTexture(mtl.maps[mk_diffuse].name, defaultDiffuseMap);
    specularMap = loadTexture(mtl.maps[mk_specular].name, defaultSpecularMap);
    emissiveMap = loadTexture(mtl.maps[mk_emissive].name, defaultEmissiveMap);
    opacityMap = loadTexture(mtl.maps[mk_opacity].name, defaultOpacityMap);
    isTransparent_ = opacityMap != defaultOpacityMap;
}

void GLMaterial::apply()
{
    glUseProgram(gProgram); //  needed to update uniforms on ATI?
glAssertError();

    glMaterialf(GL_FRONT, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT, GL_AMBIENT, &ambient.r);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, &diffuse.r);
    glMaterialfv(GL_FRONT, GL_SPECULAR, &specular.r);
    glMaterialfv(GL_FRONT, GL_EMISSION, &emissive.r);
glAssertError();

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ambientMap);
glAssertError();

    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, diffuseMap);
glAssertError();

    glActiveTexture(GL_TEXTURE2);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, specularMap);
glAssertError();

    glActiveTexture(GL_TEXTURE3);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, emissiveMap);
glAssertError();

    glActiveTexture(GL_TEXTURE4);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, opacityMap);
glAssertError();

    glProgramUniform1i(gProgram, uniformAmbientMap, 0);
    glProgramUniform1i(gProgram, uniformDiffuseMap, 1);
    glProgramUniform1i(gProgram, uniformSpecularMap, 2);
    glProgramUniform1i(gProgram, uniformEmissiveMap, 3);
    glProgramUniform1i(gProgram, uniformOpacityMap, 4);
glAssertError();

    if (opacityMap != defaultOpacityMap)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        //glEnable(GL_ALPHA_TEST);
        //glAlphaFunc(GL_GREATER, 0.04f);
        glDepthMask(GL_FALSE);
    }
    else
    {
        glDisable(GL_BLEND);
        //glDisable(GL_ALPHA_TEST);
        glDepthMask(GL_TRUE);
    }
}

void GLMaterial::release()
{
    if (--refCount_ == 0)
    {
        delete this;
    }
}

static void initDefaultMap(GLuint &map, unsigned int color)
{
    if (map != 0)
    {
        return;
    }
    glGenTextures(1, &map);
    glBindTexture(GL_TEXTURE_2D, map);
    unsigned char bytes[3] = {
        color >> 16,
        color >> 8,
        color >> 0
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &bytes);
}

static GLint compileShaderNames(char const *vsName, char const *fsName)
{
    static char buf[4096];
    char const *vs = Asset::text(vsName);
    char const *fs = Asset::text(fsName);

    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    glAssertError();
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glAssertError();
    GLuint program = glCreateProgram();
    glAttachShader(program, vShader);
    glAttachShader(program, fShader);
    glAssertError();

    GLint one = strlen(vs);
    glShaderSource(vShader, 1, &vs, &one);
    glAssertError();
    glCompileShader(vShader);
    GLint iv = 0;
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &iv);
    if (iv != GL_TRUE)
    {
        GLsizei len = 0;
        glGetShaderInfoLog(vShader, 4096, &len, buf);
        throw std::runtime_error(std::string("GLSL ") + vsName + " vertex compile error:\n" +
            std::string(&buf[0], &buf[len]));
    }
    glAssertError();

    one = strlen(fs);
    glShaderSource(fShader, 1, &fs, &one);
    glAssertError();
    glCompileShader(fShader);
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &iv);
    if (iv != GL_TRUE)
    {
        GLsizei len = 0;
        glGetShaderInfoLog(fShader, 4096, &len, buf);
        throw std::runtime_error(std::string("GLSL ") + fsName + "fragment compile error:\n" +
            std::string(&buf[0], &buf[len]));
    }
    glAssertError();

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &iv);
    if (iv != GL_TRUE)
    {
        GLsizei len = 0;
        glGetProgramInfoLog(program, 4096, &len, buf);
        throw std::runtime_error(std::string("GLSL program link error:\n") +
            vsName + " " + fsName + "\n" +
            std::string(&buf[0], &buf[len]));
    }
    glAssertError();

    return program;
}

static void compileShaders()
{
    if (gProgram)
    {
        return;
    }

    gProgram = compileShaderNames("default-vs.glsl", "default-fs.glsl");
    uniformAmbientMap = glGetUniformLocation(gProgram, "texAmbient");
    glAssertError();
    uniformDiffuseMap = glGetUniformLocation(gProgram, "texDiffuse");
    glAssertError();
    uniformSpecularMap = glGetUniformLocation(gProgram, "texSpecular");
    glAssertError();
    uniformEmissiveMap = glGetUniformLocation(gProgram, "texEmissive");
    glAssertError();
    uniformOpacityMap = glGetUniformLocation(gProgram, "texOpacity");
    glAssertError();

    gCustomProgram = compileShaderNames("custom-vs.glsl", "custom-fs.glsl");
    customMap = glGetUniformLocation(gCustomProgram, "texCustom");
    glAssertError();
}

GLContext *GLContext::context()
{
    return &ctx;
}

void GLContext::realize(int width, int height)
{
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
    glDepthRange(0.0f, 1.0f);
    initDefaultMap(defaultAmbientMap, 0x808080);
    initDefaultMap(defaultDiffuseMap, 0x808080);
    initDefaultMap(defaultSpecularMap, 0x808080);
    initDefaultMap(defaultEmissiveMap, 0x000000);
    initDefaultMap(defaultOpacityMap, 0xffffff);
    initDefaultMap(defaultCustomMap, 0xffffff);
    compileShaders();
}

void GLContext::preClear()
{
    glUseProgram(0);
}

void GLContext::preRender()
{
    glUseProgram(gProgram);
}

void GLContext::preSwap()
{
    glUseProgram(0);
}

Vec3 GLContext::size()
{
    return Vec3((float)width_, (float)height_, 1.0f);
}

BuiltMaterial *GLContext::buildMaterial(Material const &mtl)
{
    GLMaterial *ret = new GLMaterial(this);
    ret->configure(mtl);
    return ret;
}

void GLContext::beginCustom(Matrix const &modelView, int tex)
{
    glUseProgram(gCustomProgram);
    glNormal3f(0, 0, 1);
    glTexCoord2f(0.5, 0.5);
    glColor4f(1, 1, 1, 1);
    glActiveTexture(GL_TEXTURE0 + customMap);
    glBindTexture(GL_TEXTURE_2D, tex ? tex : defaultCustomMap);
    glMatrixMode(GL_MODELVIEW);
    glLoadTransposeMatrixf((float *)modelView.rows);
    glDepthMask(GL_TRUE);
glAssertError();
}

void GLContext::endCustom()
{
}
