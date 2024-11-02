#include <main.h>

int main(int argv, char ** argc)
{

    int durationSeconds = 10;

    if (argv >= 3)
    {
        std::map<std::string, std::string> args;
        std::vector<std::string> inputs;
        for (int i = 1; i < argv; i++)
        {
            inputs.push_back(argc[i]);
        }
        std::reverse(inputs.begin(), inputs.end());
        while (inputs.size() >= 2)
        {
            std::string arg = inputs.back();
            inputs.pop_back();
            args[arg] = inputs.back();
            inputs.pop_back();
        }

        if (args.find("-durationSeconds") != args.end())
        {
            durationSeconds = std::stoi(args["-durationSeconds"]);
        }
    }

    jGL::DesktopDisplay::Config conf;

    conf.VULKAN = false;

    #ifdef MACOS
    conf.COCOA_RETINA = true;
    #endif
    jGL::DesktopDisplay display(glm::ivec2(resX, resY), "GPGPU Particles", conf);
    display.setFrameLimit(60);

    glewInit();

    glm::ivec2 res = display.frameBufferSize();
    resX = res.x;
    resY = res.y;

    jGLInstance = std::move(std::make_unique<jGL::GL::OpenGLInstance>(res));

    jGL::OrthoCam camera(resX, resY, glm::vec2(0.0,0.0));

    camera.setPosition(0.0f, 0.0f);

    jLog::Log log;

    high_resolution_clock::time_point tic, tock;
    double rdt = 0.0;

    jGLInstance->setTextProjection(glm::ortho(0.0,double(resX),0.0,double(resY)));
    jGLInstance->setMSAA(1);

    RNG rng;

    float scale = camera.screenToWorld(float(resX)/float(cells), 0.0f).x;

    std::shared_ptr<jGL::Shader> shader = std::make_shared<jGL::GL::glShader>
    (
        jGL::GL::glShapeRenderer::shapeVertexShader,
        jGL::GL::glShapeRenderer::rectangleFragmentShader
    );

    shader->use();

    double delta = 0.0;
    double dt = 1e-3;
    float D = std::sqrt(2.0*eta*1.0/dt);
    jGL::ShapeRenderer::UpdateInfo uinfo;

    int l = std::ceil(std::sqrt(particles));
    float lc = 1.0/float(cells);

    std::vector<float> noise(particles, 0.0);
    std::vector<float> xythetac(particles*4, 0.0);
    std::vector<float> vxvyyomega(particles*4, 0.0);
    for (int i = 0; i < noise.size(); i++)
    {
        noise[i] = rng.nextFloat();
        xythetac[i*4] = rng.nextFloat();
        xythetac[i*4+1] = rng.nextFloat();
        xythetac[i*4+2] = rng.nextFloat();
        xythetac[i*4+3] = std::floor(xythetac[i*4]/lc)*cells+std::floor(xythetac[i*4+1]/lc);
    }
    std::cout << l*l << "\n";

    glCompute compute
    (
        {
            {"xythetac", {l, l, 4}},
            {"vxvyyomega", {l, l, 4}},
            {"noise", {l, l, 1}}
        },
        {l, l, 4},
        2,
        particlesComputeShader
    );

    compute.set("noise", noise);
    compute.set("xythetac", xythetac);
    compute.set("vxvyyomega", vxvyyomega);
    compute.sync();

    compute.shader.setUniform("n", particles);
    compute.shader.setUniform("l", l);
    compute.shader.setUniform("dt", float(dt));
    compute.shader.setUniform("diff", D);
    compute.shader.setUniform("centre", glm::vec2(0.5,0.5));

    Visualise vis(compute.outputTexture());
    vis.shader.setUniform("proj", camera.getVP());
    vis.shader.setUniform("scale", 0.33f);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);

    auto start = std::chrono::steady_clock::now();

    while (display.isOpen())
    {
        tic = high_resolution_clock::now();

        if (display.keyHasEvent(GLFW_KEY_DOWN, jGL::EventType::PRESS))
        {
            camera.incrementZoom(-1.0f);
        }
        if (display.keyHasEvent(GLFW_KEY_UP, jGL::EventType::PRESS))
        {
            camera.incrementZoom(1.0f);
        }

        if (display.keyHasEvent(GLFW_KEY_SPACE, jGL::EventType::PRESS))
        {
            paused = !paused;
        }

        if (!paused)
        {
            double mouseX, mouseY;
            display.mousePosition(mouseX,mouseY);
            compute.shader.setUniform("centre", glm::vec2(mouseX/resX, 1.0-mouseY/resY));
            compute.compute(false);
            compute.glCopyTexture
            (
                compute.outputTexture(0),
                compute.getTexture("xythetac"),
                glm::vec2(l, l)
            );

            compute.glCopyTexture
            (
                compute.outputTexture(1),
                compute.getTexture("vxvyyomega"),
                glm::vec2(l, l)
            );

            glClearColor(0.0,0.0,0.0,1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            vis.draw(particles);
        }

        delta = 0.0;
        for (int n = 0; n < 60; n++)
        {
            delta += deltas[n];
        }
        delta /= 60.0;

        if (frameId == 59)
        {
            std::cout << "FPS: " << fixedLengthNumber(1.0/delta,4) << "\n";
        }

        display.loop();

        tock = high_resolution_clock::now();

        deltas[frameId] = duration_cast<duration<double>>(tock-tic).count();
        frameId = (frameId+1) % 60;
        uinfo.scale = false;

    }

    jGLInstance->finish();

    return 0;
}