#pragma once
// Application: window, input, main loop.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "camera.h"
#include "config.h"
#include "game.h"
#include "polygon.h"
#include "room.h"
#include "shader.h"
#include "table.h"

class Application {
public:
    Application() {
        glfwSetErrorCallback([](int code, const char* msg) { std::cerr << "[GLFW " << code << "] " << msg << "\n"; });
        if (!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        try {
            init();
        } catch (...) {
            shutdown();   // a failed start-up still releases GL objects, then the window, then GLFW
            throw;
        }
    }
    ~Application() { shutdown(); }
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run() {
        double last = glfwGetTime();
        while (!glfwWindowShouldClose(window_)) {
            const double now = glfwGetTime();
            const float dt = static_cast<float>(std::min(now - last, 0.05));
            last = now;
            glfwPollEvents();
            update(dt);
            render();
            glfwSwapBuffers(window_);
        }
    }

private:
    void init() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
        window_ = glfwCreateWindow(cfg::kWindowW, cfg::kWindowH, "Card Room", nullptr, nullptr);
        if (!window_) throw std::runtime_error("Window creation failed (an OpenGL 3.3 core context is required)");
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("GLAD initialization failed");
        glfwGetFramebufferSize(window_, &fbW_, &fbH_);
        glViewport(0, 0, fbW_, fbH_);   // set explicitly instead of relying on the driver default (matters on HiDPI)

        glfwSetWindowUserPointer(window_, this);
        glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int, int action, int) {
            static_cast<Application*>(glfwGetWindowUserPointer(w))->onKey(key, action);
        });
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int fw, int fh) {
            auto* a = static_cast<Application*>(glfwGetWindowUserPointer(w));
            a->fbW_ = fw; a->fbH_ = fh;
            glViewport(0, 0, fw, fh);
        });
        // Windows and macOS stop glfwPollEvents while a window is being dragged or resized. Redrawing from the
        // refresh callback keeps the scene live instead of frozen and stretched during the resize.
        glfwSetWindowRefreshCallback(window_, [](GLFWwindow* w) {
            static_cast<Application*>(glfwGetWindowUserPointer(w))->render();
            glfwSwapBuffers(w);
        });

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);

        shader_ = std::make_unique<Shader>(kVertexSrc, kFragmentSrc);
        prims_ = std::make_unique<Primitives>();
        room_ = std::make_unique<Room>(*prims_);
        table_ = std::make_unique<Table>(*prims_);
        game_ = std::make_unique<Game>(makeCharacterDeck());

        std::cout << "\n1-4 views (2 = first-person fan) | 0 cinematic | G draw | LEFT/RIGHT select | D discard | ESC quit\n";
    }

    // GL objects must die while the context still exists. Safe to call more than once.
    void shutdown() {
        game_.reset(); table_.reset(); room_.reset(); prims_.reset(); shader_.reset();
        if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
        glfwTerminate();
    }

    void onKey(int key, int action) {
        if (action == GLFW_RELEASE) return;
        const bool press = (action == GLFW_PRESS);
        switch (key) {
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window_, GLFW_TRUE); break;
            case GLFW_KEY_1: if (press) setView(1); break;
            case GLFW_KEY_2: if (press) setView(3); break;
            case GLFW_KEY_3: if (press) setView(2); break;
            case GLFW_KEY_4: if (press) setView(4); break;
            case GLFW_KEY_0: if (press) toggleCinematic(); break;
            case GLFW_KEY_G: if (press) game_->drawRound(); break;
            case GLFW_KEY_D: if (press) game_->discardSelected(); break;
            case GLFW_KEY_LEFT:  game_->moveSelection(-1); break;
            case GLFW_KEY_RIGHT: game_->moveSelection(+1); break;
            default: break;
        }
    }
    static CameraPose poseFor(int v) {
        switch (v) {
            case 1: return topDownPose();
            case 3: return firstPersonPose();
            case 4: return tableMirrorPose();
            default: return tablePose();
        }
    }
    void setView(int v) {
        lastFixedView_ = v;
        if (active_ == &cinematic_) { camera_.setPose(cinematic_.pose()); active_ = &camera_; }
        camera_.transitionTo(poseFor(v), 1.1f);
        game_->setFanView(v == 3, firstPersonPose());   // the hand fans out only in the first-person view
    }
    void toggleCinematic() {
        if (active_ == &cinematic_) { setView(lastFixedView_); return; }
        game_->setFanView(false, firstPersonPose());
        cinematic_.begin(active_->pose());
        active_ = &cinematic_;
    }
    void update(float dt) {
        active_->update(dt);
        game_->update(dt);
        const std::string t = game_->status();
        if (t != title_) { title_ = t; glfwSetWindowTitle(window_, t.c_str()); }
    }
    void render() {
        glClearColor(0.08f, 0.09f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const float aspect = fbH_ > 0 ? float(fbW_) / float(fbH_) : 1.f;
        shader_->use();
        shader_->setInt("uTexture", 0);
        shader_->setMat4("uVP", active_->projection(aspect) * active_->view());
        room_->draw(*shader_);
        table_->draw(*shader_);
        game_->draw(*shader_);
    }

    GLFWwindow* window_ = nullptr;
    int fbW_ = cfg::kWindowW, fbH_ = cfg::kWindowH;
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Primitives> prims_;
    std::unique_ptr<Room> room_;
    std::unique_ptr<Table> table_;
    std::unique_ptr<Game> game_;
    Camera camera_{topDownPose()};
    CinematicCamera cinematic_;
    Camera* active_ = &camera_;
    int lastFixedView_ = 1;
    std::string title_;
};
