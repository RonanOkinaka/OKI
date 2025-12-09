#ifndef EXT_WINDOW_H
#define EXT_WINDOW_H

#include "GLFW/glfw3.h"
#include "oki/oki_system.h"

#include <vector>

namespace ext {
class Window : public oki::System
{
public:
    bool init(int width, int height, const char* title)
    {
        if (!glfwInit()) {
            return false;
        }

        window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window_) {
            return false;
        }

        glfwMakeContextCurrent(window_);
        glClearColor(0., 0., 0., 1.);
        return true;
    }

    // Input should be implemented with signals instead (but I'm lazy)
    bool key_pressed(int glfwKey) { return glfwGetKey(window_, glfwKey) == GLFW_PRESS; }

private:
    void step(oki::SystemManager&, oki::SystemOptions& opts) override
    {
        glfwSwapBuffers(window_);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwPollEvents();

        if (glfwWindowShouldClose(window_)) {
            // Similarly, good design would emit an exit signal
            opts.exit(0);
            return;
        }

        glfwMakeContextCurrent(window_);
    }

    GLFWwindow* window_ = nullptr;
};
}

#endif // EXT_WINDOW_H
