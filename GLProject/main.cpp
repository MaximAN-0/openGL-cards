// =============================================================================
//  Card Room - modern OpenGL (3.3 core), C++17, OOP design
//
//  Controls
//    1 / 2        top-down view / table view (camera slides between them)
//    3            first-person view: your hand is shown as a fan in front of the camera
//    4            table view mirrored to the opposite corner (behind the opponent)
//    0            toggle cinematic "idle look-around" camera
//    G            both players draw one card (opponent is delayed by 0.5 s)
//    LEFT / RIGHT select a card in your hand (it slides up)
//    D            discard the selected card to the discard pile
//    ESC          quit
//
//  Assets are OPTIONAL. If a PNG is missing a procedural texture is generated:
//    assets/wood.png  assets/wallpaper.png  assets/poster.png (needs alpha for the torn edge)
//    assets/cards/back.png  assets/cards/<name>.png
// =============================================================================
#include <exception>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "application.h"

int main() {
    try {
        Application app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        glfwTerminate();
        return 1;
    }
    return 0;
}
