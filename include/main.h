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
int particles = 9999873;
float eta = 1.0;

uint8_t frameId = 0;
double deltas[60];

bool debug = false;
bool paused = false;

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
    Visualise(GLuint particlesTexture, GLuint obstaclesTexture)
    : particlesTexture(particlesTexture), obstaclesTexture(obstaclesTexture)
    {
        glGenVertexArrays(1, &pvao);
        glBindVertexArray(pvao);
        glGenBuffers(1, &pvbo);
        glBindBuffer(GL_ARRAY_BUFFER, pvbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*2,
            &p[0],
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

        glGenVertexArrays(1, &qvao);
        glBindVertexArray(qvao);
        glGenBuffers(1, &qvbo);
        glBindBuffer(GL_ARRAY_BUFFER, qvbo);
        glBufferData
        (
            GL_ARRAY_BUFFER,
            sizeof(float)*6*4,
            &quad[0],
            GL_STATIC_DRAW
        );
        glEnableVertexAttribArray(0);
        glVertexAttribPointer
        (
            0,
            4,
            GL_FLOAT,
            false,
            4*sizeof(float),
            0
        );
        glVertexAttribDivisor(0,0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void drawParticles(uint64_t particles, float scale, glm::mat4 proj)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, particlesTexture);
        shader = jGL::GL::glShader(vertexShader, fragmentShader);
        shader.compile();
        shader.use();
        shader.setUniform("tex", jGL::Sampler2D(1));
        shader.setUniform("n", int(std::sqrt(particles)));
        shader.setUniform("scale", scale);
        shader.setUniform("proj", proj);

        glBindVertexArray(pvao);
        glBindBuffer(GL_ARRAY_BUFFER, pvbo);
        glDrawArraysInstanced(GL_POINTS, 0, 1, particles);
        glBindVertexArray(0);
    }

    void drawObstacles(uint64_t obstacles, float scale, glm::mat4 proj)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, obstaclesTexture);
        shader = jGL::GL::glShader(obstacleVertexShader, obstacleFragmentShader);
        shader.compile();
        shader.use();
        shader.setUniform("tex", jGL::Sampler2D(1));
        shader.setUniform("proj", proj);

        glBindVertexArray(qvao);
        glBindBuffer(GL_ARRAY_BUFFER, qvbo);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    jGL::GL::glShader shader;
    GLuint particlesTexture, obstaclesTexture, pvao, pvbo, qvao, qvbo;
    float p[2] =
    {
        0.0f,0.0f
    };

        float quad[6*4] =
    {
        -1.0, -1.0, 0.0, 0.0,
         1.0, -1.0, 1.0, 0.0,
         1.0,  1.0, 1.0, 1.0,
        -1.0, -1.0, 0.0, 0.0,
        -1.0,  1.0, 0.0, 1.0,
         1.0,  1.0, 1.0, 1.0
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
    "   float theta = atan(stateij.w, stateij.z);\n"
    "   if (theta < 0.0) { theta +=  2.0*3.14159; }"
    "   o_colour = vec4(cmap(theta/(2.0*3.14159)), 1.0);\n"
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

    const char * obstacleVertexShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location = 0) in vec4 a_position;\n"
    "uniform mat4 proj;\n"
    "out vec2 o_texCoords;\n"
    "void main(){\n"
    "   gl_Position = vec4(a_position.xy,0.0,1.0);\n"
    "   o_texCoords = a_position.zw;\n"
    "}";

    const char * obstacleFragmentShader =
    "#version " GLSL_VERSION "\n"
    "uniform highp sampler2D tex;\n"
    "in vec2 o_texCoords;\n"
    "out vec4 colour;\n"
    "void main(void){\n"
    "   vec4 t = texture(tex, o_texCoords);\n"
    "   if (t.r == 0) { discard; }\n"
    "   colour = vec4(1.0,1.0,1.0,t.r);\n"
    "}";
};

const char * particlesComputeShader =
    "#version " GLSL_VERSION "\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "in vec2 o_texCoords;\n"
    "layout(location=0) out vec4 output1;\n"
    "layout(location=1) out vec4 output2;\n"
    "uniform highp sampler2D xyvxvy;\n"
    "uniform highp sampler2D noise;\n"
    "uniform highp sampler2D obstacles;\n"
    "uniform int n; uniform int l;\n"
    "uniform vec2 centre; uniform vec2 res;\n"
    "uniform float dt; uniform float diff;\n"
    "uniform int steps;\n"
    "float random(vec2 st){\n"
    "    return clamp(fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123), 0.001, 1.0);\n"
    "}\n"
    "void main(){\n"
    "    int i = int(o_texCoords.x*float(l)); int j = int(o_texCoords.y*float(l));\n"
    "    vec4 xyvxvy = texture(xyvxvy, o_texCoords);\n"
    "    float d = texture(noise, o_texCoords).r;\n"
    "    vec2 p = xyvxvy.xy; vec2 v = xyvxvy.zw;\n"
    "    float dtau = dt/float(steps);\n"
    "    vec2 f = vec2(0.0, 0.0);\n"
    "    for (int k = 0; k < steps; k++){"
    "       float dx = (random(d*k*vec2(p.x,v.x))-0.5); float dy = (random(vec2(p.y,v.y)*d*k)-0.5);\n"
    "       f = 20.0*vec2(dx, dy);"
    "       float obs = texture(obstacles, p).r;\n"
    "       vec2 r = centre-p;\n"
    "       float dist = length(r);\n"
    "       if (obs == 1.0) {"
    "            int search = 2;"
    "            vec2 dir = -v;\n"
    "            float mag = length(v)*dt;\n"
    "            for (int ix = -search; ix < search; ix++){"
    "                for (int iy = -search; iy < search; iy++){"
    "                    vec2 c = p+vec2(float(ix)/res.x, float(iy)/res.y);\n"
    "                    float obsn = texture(obstacles, c).r;\n"
    "                    if (obsn == 0.0) {"
    "                        dir = vec2(float(ix)/res.x, float(iy)/res.y);\n"
    "                        mag = length(dir);\n"
    "                        break;\n"
    "                    }"
    "                }"
    "            }"
    "            vec2 r = p-(floor(p*res)/res+0.5/res);\n"
    "            float d = dot(r, r);\n"
    "            p += mag*dir/length(dir);\n"
    "            v = v - 2.0*(dot(v, r)/d)*r+f*dt;\n"
    "        }"
    "        else {\n"
    "           d = length(r);\n"
    "           f += 10.0*r/(d*d);\n"
    "           v += dt*f;\n"
    "       }"
    "       p += dt*v;\n"
    "    }"
    "    output1 = vec4(p, v);\n"
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

void place(std::vector<float> & into, int i, int j, int brush, int l)
{
    for (int n = -brush; n <= brush; n++)
    {
        for (int m = -brush; m <= brush; m++)
        {
            int ix = (n+i) % l;
            int iy = (m+j) % l;
            if (ix < 0) { ix += l; }
            if (iy < 0) { iy += l; }
            into[iy*l+ix] = 1.0;
        }
    }
}

#endif /* MAIN_H */
