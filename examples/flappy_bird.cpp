#include "stopwatch.h"
#include "window.h"

#include "GL/gl.h"
#include "GLFW/glfw3.h"
#include "oki/oki_ecs.h"

#include <random>

struct Rect
{
    float x1, x2, y1, y2;
};

struct PhysicsVec
{
    float velX, velY, accX, accY;
};

struct Color
{
    float r, g, b;
};

struct PipeTag { };

class SimpleRenderer
    : public oki::EngineSystem<SimpleRenderer>
{
public:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        glBegin(GL_QUADS);
        engine.for_each<Rect, Color>(this->render_rect_);
        glEnd();
    }

private:
    static void render_rect_(oki::Entity, Rect rect, Color color) noexcept
    {
        glColor3f(color.r, color.g, color.b);
        glVertex2f(rect.x1, rect.y1);
        glVertex2f(rect.x2, rect.y1);
        glVertex2f(rect.x2, rect.y2);
        glVertex2f(rect.x1, rect.y2);
    }
};

class PhysicsSystem
    : public oki::EngineSystem<PhysicsSystem>
{
public:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        float elapsed = frametime_.start();

        engine.for_each<Rect, PhysicsVec>([=](auto, Rect& rect, PhysicsVec& vec)
        {
            rect.x1 += vec.velX * elapsed;
            rect.x2 += vec.velX * elapsed;
            rect.y1 += vec.velY * elapsed;
            rect.y2 += vec.velY * elapsed;
            vec.velX += vec.accX * elapsed;
            vec.velY += vec.accY * elapsed;
        });
    }

private:
    ext::StopWatch frametime_;
};

class PipeSystem
    : public oki::SimpleEngineSystem
{
public:
    PipeSystem()
        : randSrc_(std::random_device{ }()) { }

private:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        auto now = std::chrono::steady_clock::now();
        if (pipeSpawn_.count() > 2.f)
        {
            this->create_pipe(engine);
        }

        std::vector<oki::Entity> toDelete;
        engine.for_each<PipeTag, Rect>([&](auto entity, auto, auto rect) {
            if (rect.x2 < -1.1f)
            {
                toDelete.push_back(entity);
            }
        });

        for (auto entity : toDelete)
        {
            engine.remove_component<Rect>(entity);
            engine.remove_component<PhysicsVec>(entity);
            engine.remove_component<Color>(entity);
            engine.remove_component<PipeTag>(entity);
            engine.destroy_entity(entity);
        }
    }

    void create_pipe(oki::Engine& engine)
    {
        std::uniform_real_distribution<float> heightGen{ -0.4, 0.2 };
        float height = heightGen(randSrc_);

        Rect r1{ 1.1f, 1.2f, -1.1f, height };
        Rect r2{ 1.1f, 1.2f, height + 0.6f, 1.1f };

        auto left = PhysicsVec{ -0.2f, 0.f, 0.f, 0.f };
        auto green = Color{ 0.f, 1.f, 0.2f };

        auto pipe1 = engine.create_entity();
        auto pipe2 = engine.create_entity();

        // TODO: allow binding multiple components at once
        engine.bind_component(pipe1, r1);
        engine.bind_component(pipe1, left);
        engine.bind_component(pipe1, green);
        engine.bind_component(pipe1, PipeTag{ });

        engine.bind_component(pipe2, r2);
        engine.bind_component(pipe2, left);
        engine.bind_component(pipe2, green);
        engine.bind_component(pipe2, PipeTag{ });

        pipeSpawn_.start();
    }

    ext::StopWatch pipeSpawn_;

    std::default_random_engine randSrc_;
};

class BirdSystem
    : public oki::SimpleEngineSystem
{
public:
    BirdSystem(oki::Entity bird, ext::Window& window)
        : window_(window), bird_(bird) { }

private:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        auto [rect, phys] = engine.get_components<Rect, PhysicsVec>(bird_);

        if (input_.count() > 0.45f && window_.key_pressed(GLFW_KEY_SPACE))
        {
            input_.start();
            phys.velY = 0.5f;
        }
    }

    ext::Window& window_;

    oki::Entity bird_;

    ext::StopWatch input_;
};

int main()
{
    oki::Engine engine;

    auto bird = engine.create_entity();
    engine.bind_component(bird, Rect{ -0.23, -0.27, -0.02, 0.02 });
    engine.bind_component(bird, Color{ 1., 0.5, 0.12 });
    engine.bind_component(bird, PhysicsVec{ 0., 0., 0., -0.7 });

    ext::Window window;
    if (!window.init(640, 480, "Flappy Bird"))
    {
        return 1;
    }

    engine.add_system(window);

    SimpleRenderer renderer;
    engine.add_system(renderer);

    PhysicsSystem physics;
    engine.add_system(physics);

    PipeSystem pSystem;
    engine.add_system(pSystem);

    BirdSystem bSystem{ bird, window };
    engine.add_system(bSystem);

    return engine.run();
}
