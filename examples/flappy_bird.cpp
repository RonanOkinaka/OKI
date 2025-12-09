#include "stopwatch.h"
#include "window.h"

// Platform-dependent trickery
#ifdef __APPLE__
#include "OpenGL/gl.h"
#else
#include "GL/gl.h"
#endif

#include "GLFW/glfw3.h"
#include "oki/oki_ecs.h"

#include <initializer_list>
#include <random>
#include <vector>

// The following four classes are the components we use to implement the game.
struct Rect
{
    float x1, x2, y1, y2;

    bool overlaps(const Rect& r2) const noexcept
    {
        return (x1 <= r2.x2) && (y1 <= r2.y2) && (r2.x1 <= x2) && (r2.y1 <= y2);
    }

    bool contains(const Rect& r2) const noexcept
    {
        return (x1 <= r2.x1) && (r2.x2 <= x2) && (y1 <= r2.y1) && (r2.y2 <= y2);
    }
};

struct PhysicsVec
{
    float velX, velY, accX, accY;
};

struct Color
{
    float r, g, b;
};

/*
 * It is sometimes useful to "tag" an entity with a type in order to more
 * easily differentiate them from others.
 * In this case, the tag gets used to delineate between the bounding
 * rectangle of a pipe and the player.
 */
struct PipeTag
{ };

/*
 * This is an event, emitted when the player dies. It is useful to include
 * a reference to the engine for easier cleanup.
 */
struct GameOverEvent
{
    oki::Engine& engine;
};

/*
 * This is a system, responsible for rendering colored rectangles.
 * It is marked as such by inheriting from oki::EngineSystem<>, and in this
 * case is making use of the optional CRTP.
 */
class SimpleRenderer : public oki::EngineSystem<SimpleRenderer>
{
public:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        // To render, we simply iterate over each entity's color and rectangle!
        // (Immediate mode is sufficient here because high-performance rendering
        // isn't the goal of the example.)
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

class PhysicsSystem : public oki::EngineSystem<PhysicsSystem>
{
public:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        float elapsed = frametime_.start();

        // Similarly, we simply iterate over bounding rectangles and the
        // movement characteristics (velocity + acceleration) to calculate our
        // "physics." (Accuracy is neither achieved nor important here.)
        engine.for_each<Rect, PhysicsVec>([=](auto, Rect& rect, PhysicsVec& vec) {
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
    // Note that the CRTP is optional! This system does not use it.
    : public oki::SimpleEngineSystem
{
public:
    PipeSystem()
        : randSrc_(std::random_device {}())
    {
    }

private:
    void step(oki::Engine& engine, oki::SystemOptions&) override
    {
        auto now = std::chrono::steady_clock::now();
        if (pipeSpawn_.count() > 2.f) {
            this->create_pipe_(engine);
        }

        // Here is one limitation of the library: cleanup
        // We cannot remove components while iterating over them...
        std::vector<oki::Entity> toDelete;
        engine.for_each<PipeTag, Rect>([&](auto entity, auto, auto rect) {
            if (rect.x2 < -1.1f) {
                toDelete.push_back(entity);
            }
        });

        // ...and destroying an entity will leak its components
        for (auto entity : toDelete) {
            engine.remove_component<Rect>(entity);
            engine.remove_component<PhysicsVec>(entity);
            engine.remove_component<Color>(entity);
            engine.remove_component<PipeTag>(entity);
            engine.destroy_entity(entity);
        }
    }

    void create_pipe_(oki::Engine& engine)
    {
        std::uniform_real_distribution<float> heightGen { -0.4, 0.2 };
        float height = heightGen(randSrc_);

        Rect r1 { 1.1f, 1.2f, -1.1f, height };
        Rect r2 { 1.1f, 1.2f, height + 0.6f, 1.1f };

        auto left = PhysicsVec { -0.2f, 0.f, 0.f, 0.f };
        auto green = Color { 0.f, 1.f, 0.2f };

        auto pipe1 = engine.create_entity();
        auto pipe2 = engine.create_entity();

        // TODO: allow binding multiple components at once
        engine.bind_component(pipe1, r1);
        engine.bind_component(pipe1, left);
        engine.bind_component(pipe1, green);
        engine.bind_component(pipe1, PipeTag {});

        engine.bind_component(pipe2, r2);
        engine.bind_component(pipe2, left);
        engine.bind_component(pipe2, green);
        engine.bind_component(pipe2, PipeTag {});

        pipeSpawn_.start();
    }

    ext::StopWatch pipeSpawn_;
    std::default_random_engine randSrc_;
};

// This class is both a system and an observer (listening for a game-over)
class BirdSystem : public oki::SimpleEngineSystem, public oki::Observer<GameOverEvent>
{
public:
    // Also, we take a handle to the player entity; this is not "pure" ECS-style
    // but simplifies our design considerably
    BirdSystem(oki::Entity bird, ext::Window& window)
        : window_(window)
        , bird_(bird)
        , screenBox_({ -1, 1, -1, 1 })
    {
    }

private:
    void step(oki::Engine& engine, oki::SystemOptions& opts) override
    {
        auto [rect, phys] = engine.get_components<Rect, PhysicsVec>(bird_);

        if (input_.count() > 0.45f && window_.key_pressed(GLFW_KEY_SPACE)) {
            input_.start();
            phys.velY = 0.5f;
        }

        engine.for_each<PipeTag, Rect>([&rect, &engine](auto, auto, auto pipeRect) {
            if (pipeRect.overlaps(rect)) {
                engine.send(GameOverEvent { engine });
            }
        });

        if (!screenBox_.contains(rect)) {
            engine.send(GameOverEvent { engine });
        }
    }

    // Turn the bird red and disconnect on game-over
    void observe(GameOverEvent event, oki::ObserverOptions& opts) override
    {
        event.engine.get_component<Color>(bird_) = { 1.0, 0.0, 0.0 };

        opts.disconnect();
    }

    ext::Window& window_;
    ext::StopWatch input_;

    Rect screenBox_;
    oki::Entity bird_;
};

// Sometimes, we want to handle events externally; this class is an
// observer but not a system.
// It simply removes a group of systems when the game-over is emitted.
class RemoveOnGameOver : public oki::Observer<GameOverEvent>
{
public:
    RemoveOnGameOver(std::initializer_list<oki::Handle>&& handles)
        : systems_(handles)
    {
    }

private:
    std::vector<oki::Handle> systems_;

    void observe(GameOverEvent event, oki::ObserverOptions& opts) override
    {
        for (auto system : systems_) {
            event.engine.remove_system(system);
        }

        opts.disconnect();
    }
};

int main()
{
    // Once our systems are set up, running the game is easy!
    // We first create the engine
    oki::Engine engine;

    // Then, we create some important entities (in this case, the player)
    auto bird = engine.create_entity();
    engine.bind_component(bird, Rect { -0.27, -0.23, -0.02, 0.02 });
    engine.bind_component(bird, Color { 1., 0.5, 0.12 });
    engine.bind_component(bird, PhysicsVec { 0., 0., 0., -0.7 });

    // Create and add the systems
    ext::Window window;
    if (!window.init(640, 480, "Flappy Bird")) {
        return 1;
    }

    engine.add_system(window);

    SimpleRenderer renderer;
    engine.add_system(renderer);

    PhysicsSystem physics;
    auto physHandle = engine.add_system(physics);

    PipeSystem pSystem;
    auto pipeHandle = engine.add_system(pSystem);

    BirdSystem bSystem { bird, window };
    auto birdHandle = engine.add_system(bSystem);
    // We also connect listeners as necessary
    engine.connect<GameOverEvent>(bSystem);

    RemoveOnGameOver remover { physHandle, pipeHandle, birdHandle };
    engine.connect<GameOverEvent>(remover);

    // Then, run! The engine will handle everything from here
    return engine.run();
}
