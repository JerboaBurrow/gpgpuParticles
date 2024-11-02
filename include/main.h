#ifndef MAIN_H
#define MAIN_H

#include <jGL/jGL.h>
#include <jGL/OpenGL/openGLInstance.h>
#include <jGL/OpenGL/Shader/glShader.h>
#include <jGL/shape.h>

#include <logo.h>
#include <jGL/Display/desktopDisplay.h>
#include <jGL/orthoCam.h>

#include <jLog/jLog.h>

#include <rand.h>
#include <algorithm>
#include <chrono>
#include <sstream>

#include <glCompute.h>

using namespace std::chrono;

int resX = 1024;
int resY = 1024;

int cells = 128;
int particles = 10000000;
float eta = 0.1;

uint8_t frameId = 0;
double deltas[60];

bool debug = false;
bool paused = true;

std::unique_ptr<jGL::jGLInstance> jGLInstance;

std::string fixedLengthNumber(double x, unsigned length)
{
    std::string d = std::to_string(x);
    std::string dtrunc(length,' ');
    for (unsigned c = 0; c < dtrunc.length(); c++/*ayy lmao*/)
    {

        if (c >= d.length())
        {
            dtrunc[c] = '0';
        }
        else
        {
            dtrunc[c] = d[c];
        }
    }
    return dtrunc;
}

struct Visualise
{
    Visualise(GLuint texture)
    : texture(texture)
    {
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*2,
            &quad[0],
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer
        (
            0,
            2,
            GL_FLOAT,
            false,
            2*sizeof(float),
            0
        );
        glVertexAttribDivisor(0,0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void draw(uint64_t particles)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture);
        shader.use();
        shader.setUniform("tex", jGL::Sampler2D(1));
        shader.setUniform("n", int(std::sqrt(particles)));

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glDrawArraysInstanced(GL_POINTS, 0, 1, particles);
        glBindVertexArray(0);
    }

    jGL::GL::glShader shader;
    GLuint texture, vao, vbo;
    float quad[2] =
    {
        0.0f,0.0f
    };
    const char * vertexShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) in vec2 a_position;\n"
    "out vec4 o_colour;\n"
    "uniform highp sampler2D tex;\n"
    "uniform mat4 proj;\n"
    "uniform int n;\n"
    "uniform float scale;\n"
    "float poly(float x, float p0, float p1, float p2, float p3, float p4){\n"
    "    float x2 = x*x; float x4 = x2*x2; float x3 = x2*x;\n"
    "    return clamp(p0+p1*x+p2*x2+p3*x3+p4*x4,0.0,1.0);\n"
    "}\n"
    "vec3 cmap(float t)\n{"
    "    return vec3( poly(t,0.91, 3.74, -32.33, 57.57, -28.99), poly(t,0.2, 5.6, -18.89, 25.55, -12.25), poly(t,0.22, -4.89, 22.31, -23.58, 5.97) );\n"
    "}\n"
    "vec2 particleNumberToTex(int p, int n){\n"
        "float i = floor(float(p)/float(n)); float j = mod(float(p),float(n));\n"
        "return vec2( (j+0.5)/float(n), (i+0.5)/float(n) );\n"
    "}\n"
    "void main(){\n"
    "   vec2 coords = particleNumberToTex(gl_InstanceID,n);\n"
    "   vec4 stateij = texture(tex, coords);\n"
    "   o_colour = vec4(cmap(stateij.z/(2.0*3.14159)), 1.0);\n"
    "   vec4 pos = proj*vec4(stateij.x,stateij.y,0.0,1.0);\n"
    "   gl_Position = vec4(a_position.xy+pos.xy,0.0,1.0);\n"
    "   gl_PointSize = scale;\n"
    "}";

    const char * fragmentShader =
    "#version " GLSL_VERSION "\n"
    "in vec4 o_colour;\n"
    "out vec4 colour;\n"
    "void main(void){\n"
    "   vec2 circCoord = 2.0 * gl_PointCoord - 1.0;"
    "   float dd = length(circCoord);\n"
    "   float alpha = 1.0-smoothstep(0.9,1,dd);\n"
    "   colour = vec4(o_colour.rgb,alpha);\n"
    "   if (colour.a == 0.0){discard;}\n"
    "}";
};

const char * particlesComputeShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out vec4 output1;\n"
    "layout(location=1) out vec4 output2;\n"
    "uniform highp sampler2D xythetac;\n"
    "uniform highp sampler2D vxvyyomega;\n"
    "uniform highp sampler2D noise;\n"
    "uniform int n;\n"
    "uniform int l;\n"
    "uniform vec2 centre;\n"
    "uniform float dt;\n"
    "uniform float diff;\n"
    "float random(vec2 st){\n"
    "    return clamp(fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123), 0.001, 1.0);\n"
    "}\n"
    "void main(){\n"
    "    int i = int(o_texCoords.x*float(l)); int j = int(o_texCoords.y*float(l));\n"
    "    vec4 xythetac = texture(xythetac, o_texCoords);\n"
    "    vec4 vxvyyomega = texture(vxvyyomega, o_texCoords);\n"
    "    float d = texture(noise, o_texCoords).r;\n"
    "    float dx = (random(d*xythetac.xy)-0.5); float dy = (random(d*xythetac.yx)-0.5);\n"
    "    vec2 r = centre-xythetac.xy;\n"
    "    float dist = length(r);\n"
    "    float fx = 0.0; float fy = 0.0;"
    "    if (dist > 0.5) {"
    "       fx = 10.0*r.x+dx; fy = 10.0*r.y+dy;\n"
    "    }"
    "    else {"
    "       if (dist < 0.001) { dist = 0.001; }"
    "       float d3 = dist*dist*dist;\n"
    "       fx = r.x/d3+dx; fy = r.y/d3+dy;\n"
    "    }"
    "    vec2 v = vxvyyomega.xy+vec2(fx, fy)*dt;\n"
    "    vec2 xy = xythetac.xy+vxvyyomega.xy*dt;\n"
    "    float theta = atan(v.y, v.x);\n"
    "    if (theta < 0.0) { theta +=  2.0*3.14159; }"
    "    output1 = vec4(xy, theta, 0.0);\n"
    "    output2 = vec4(v, 0.0, 0.0);\n"
    "}";

float clamp(float x, float low, float high)
{
    return std::min(std::max(x, low), high);
}

float poly(float x, float p0, float p1, float p2, float p3, float p4)
{
   float x2 = x*x; float x4 = x2*x2; float x3 = x2*x;
   return clamp(p0+p1*x+p2*x2+p3*x3+p4*x4,0.0,1.0);
}

glm::vec3 cmap(float t)
{
    return glm::vec3( poly(t,0.91, 3.74, -32.33, 57.57, -28.99), poly(t,0.2, 5.6, -18.89, 25.55, -12.25), poly(t,0.22, -4.89, 22.31, -23.58, 5.97) );
}

#endif /* MAIN_H */
