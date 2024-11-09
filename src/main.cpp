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

    // glm::ivec2 res = display.frameBufferSize();
    // resX = res.x;
    // resY = res.y;

    jGLInstance = std::move(std::make_unique<jGL::GL::OpenGLInstance>(glm::ivec2(resX,resY)));

    jGL::OrthoCam camera(resX, resY, glm::vec2(0.0,0.0));

    camera.setPosition(0.0f, 0.0f);

    jLog::Log log;

    high_resolution_clock::time_point tic, tock;
    double rdt = 0.0;

    jGLInstance->setTextProjection(glm::ortho(0.0,double(resX),0.0,double(resY)));
    jGLInstance->setMSAA(0);

    RNG rng;

    float scale = camera.screenToWorld(float(resX)/float(cells), 0.0f).x;

    std::shared_ptr<jGL::Shader> shader = std::make_shared<jGL::GL::glShader>
    (
        jGL::GL::glShapeRenderer::shapeVertexShader,
        jGL::GL::glShapeRenderer::rectangleFragmentShader
    );

    shader->use();

    double delta = 0.0;
    double dt = 1e-4;
    jGL::ShapeRenderer::UpdateInfo uinfo;

    int l = std::ceil(std::sqrt(particles));
    float lc = 1.0/float(cells);

    std::vector<float> noise(particles, 0.0);
    std::vector<float> xyvxvy(particles*4, 0.0);
    std::vector<float> obstacles(resX*resY, 0.0);
    for (int i = 0; i < noise.size(); i++)
    {
        noise[i] = rng.nextFloat();
        float t = rng.nextFloat()*2.0*3.14159;
        xyvxvy[i*4] = std::cos(t)*(rng.nextFloat()*(1.0-0.5)+0.5)+0.5;
        xyvxvy[i*4+1] = std::sin(t)*(rng.nextFloat()*(1.0-0.5)+0.5)+0.5;
    }
    std::cout << l*l << "\n";

    glCompute compute
    (
        {
            {"xyvxvy", {l, l, 4}},
            {"noise", {l, l, 1}},
            {"obstacles", {resX, resY, 1}}
        },
        {l, l, 4},
        2,
        particlesComputeShader
    );

    compute.set("noise", noise);
    compute.set("xyvxvy", xyvxvy);
    compute.set("obstacles", obstacles);
    compute.sync();

    compute.shader.setUniform("n", particles);
    compute.shader.setUniform("l", l);
    compute.shader.setUniform("dt", float(dt));
    compute.shader.setUniform("diff", eta);
    compute.shader.setUniform("centre", glm::vec2(0.5,0.5));
    compute.shader.setUniform("res", glm::vec2(resX, resY));
    compute.shader.setUniform("steps", 4);

    Visualise vis(compute.getTexture("xyvxvy"), compute.getTexture("obstacles"));
    float pscale = 1.0f;
    float oscale = 1.0/resX;
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);

    bool placeing = false;

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

        if (display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::PRESS) || display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::HOLD))
        {
            placeing = true;
        }
        else if (display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::RELEASE))
        {
            placeing = false;
        }

        if (placeing)
        {
            double mouseX, mouseY;
            display.mousePosition(mouseX,mouseY);
            place(obstacles, int(mouseX), int(resY-mouseY), 16, resX);
            compute.set("obstacles", obstacles);
            compute.sync("obstacles");
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
                compute.getTexture("xyvxvy"),
                glm::vec2(l, l)
            );
        }
        glClearColor(0.0,0.0,0.0,1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        vis.drawParticles(particles, pscale, camera.getVP());
        vis.drawObstacles(obstacles.size(), oscale, camera.getVP());

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