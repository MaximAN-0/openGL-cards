#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <string>
#include <vector>

constexpr int WINDOW_W = 1280;
constexpr int WINDOW_H = 800;
constexpr int MAX_DRAWS = 6;

const char* VERTEX_SHADER = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=uMVP*vec4(aPos,1.0); }
)";

const char* FRAGMENT_SHADER = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec3 uTint;
uniform int uUseTexture;
void main(){
    vec4 tex = (uUseTexture == 1) ? texture(uTexture,vUV) : vec4(1.0);
    FragColor = vec4(tex.rgb*uTint, tex.a);
}
)";

struct Vertex { float x,y,z,u,v; };

class Shader {
public:
    GLuint id{};
    explicit Shader(const char* vs, const char* fs) {
        GLuint v=glCreateShader(GL_VERTEX_SHADER), f=glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(v,1,&vs,nullptr); glCompileShader(v);
        glShaderSource(f,1,&fs,nullptr); glCompileShader(f);
        GLint ok=0; glGetShaderiv(v,GL_COMPILE_STATUS,&ok);
        if(!ok){ char log[1024]; glGetShaderInfoLog(v,1024,nullptr,log); std::cerr<<"Vertex shader: "<<log<<"\n"; }
        glGetShaderiv(f,GL_COMPILE_STATUS,&ok);
        if(!ok){ char log[1024]; glGetShaderInfoLog(f,1024,nullptr,log); std::cerr<<"Fragment shader: "<<log<<"\n"; }
        id=glCreateProgram(); glAttachShader(id,v); glAttachShader(id,f); glLinkProgram(id);
        glDeleteShader(v); glDeleteShader(f);
    }
    void use() const { glUseProgram(id); }
    void mat4(const char* n,const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(id,n),1,GL_FALSE,glm::value_ptr(m)); }
    void vec3(const char* n,const glm::vec3& v) const { glUniform3fv(glGetUniformLocation(id,n),1,glm::value_ptr(v)); }
    void integer(const char* n,int v) const { glUniform1i(glGetUniformLocation(id,n),v); }
};

class Texture {
public:
    GLuint id{};
    bool load(const std::string& path) {
        int w,h,c; stbi_set_flip_vertically_on_load(true);
        unsigned char* data=stbi_load(path.c_str(),&w,&h,&c,0);
        if(!data){ std::cerr<<"Could not load texture: "<<path<<"\n"; return false; }
        glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
        GLenum fmt=(c==4)?GL_RGBA:GL_RGB;
        glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        stbi_image_free(data); return true;
    }
    void bind() const { glBindTexture(GL_TEXTURE_2D,id); }
};

class Renderer {
    GLuint vao{},vbo{};
public:
    Renderer() {
        const Vertex quad[]={{-0.5f,-0.5f,0,0,0},{0.5f,-0.5f,0,1,0},
          {0.5f,0.5f,0,1,1},{-0.5f,0.5f,0,0,1}};
        const unsigned int idx[]={0,1,2,0,2,3};
        GLuint ebo; glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo); glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));
    }
    void plane(const Shader& s,const glm::mat4& vp,const glm::vec3& pos,const glm::vec3& scale,
               const glm::vec3& tint,const Texture* tex=nullptr,float rotX=0) const {
        glm::mat4 m(1); m=glm::translate(m,pos); m=glm::rotate(m,glm::radians(rotX),glm::vec3(1,0,0)); m=glm::scale(m,scale);
        s.mat4("uMVP",vp*m); s.vec3("uTint",tint); s.integer("uUseTexture",tex?1:0);
        if(tex) tex->bind(); glBindVertexArray(vao); glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
    }
    void box(const Shader& s,const glm::mat4& vp,const glm::vec3& p,const glm::vec3& size,const glm::vec3& color) const {
        // Six simple faces; adequate for furniture and table blocks.
        plane(s,vp,p+glm::vec3(0,size.y/2,0),glm::vec3(size.x,size.z,1),color,nullptr,-90);
        plane(s,vp,p-glm::vec3(0,size.y/2,0),glm::vec3(size.x,size.z,1),color,nullptr,90);
        plane(s,vp,p+glm::vec3(0,0,size.z/2),glm::vec3(size.x,size.y,1),color);
        plane(s,vp,p-glm::vec3(0,0,size.z/2),glm::vec3(size.x,size.y,1),color);
        plane(s,vp,p+glm::vec3(size.x/2,0,0),glm::vec3(size.z,size.y,1),color,nullptr,0);
        plane(s,vp,p-glm::vec3(size.x/2,0,0),glm::vec3(size.z,size.y,1),color,nullptr,0);
    }
};

struct Card {
    std::string name;
    Texture texture;
};

class Deck {
    std::vector<Card> cards;
    std::mt19937 rng{std::random_device{}()};
public:
    void add(const std::string& name,const std::string& path) {
        Card c; c.name=name; c.texture.load(path); cards.push_back(std::move(c));
    }
    void shuffle(){ std::shuffle(cards.begin(),cards.end(),rng); }
    bool draw(Card& out){
        if(cards.empty()) return false;
        out = std::move(cards.back());
        cards.pop_back();
        return true;
    }
    std::size_t remaining() const { return cards.size(); }
};

class Game {
    Deck deck;
    std::vector<Card> hand;
    int view=1;
public:
    Game() {
        deck.add("Ember","assets/cards/ember.png");
        deck.add("Frost","assets/cards/frost.png");
        deck.add("Moss","assets/cards/moss.png");
        deck.add("Nova","assets/cards/nova.png");
        deck.add("Sol","assets/cards/sol.png");
        deck.add("Shade","assets/cards/shade.png");
        deck.shuffle();
    }
    void setView(int v){ view=v; }
    int getView() const { return view; }
    void drawCard(){
        if(hand.size()>=MAX_DRAWS) return;
        Card card;
        if(deck.draw(card)) hand.push_back(std::move(card));
    }
    const std::vector<Card>& playerHand() const { return hand; }
    std::size_t remaining() const { return deck.remaining(); }
};

class CardRoom {
    Shader shader{VERTEX_SHADER,FRAGMENT_SHADER};
    Renderer renderer;
    Texture wood,wallpaper,poster;
    Game game;
    glm::mat4 cameraVP() const {
        const bool topDown = (game.getView()==1);
        glm::vec3 eye = topDown ? glm::vec3(0,10.5f,0.01f) : glm::vec3(7.5f,7.0f,10.5f);
        glm::vec3 target(0,0.8f,0);
        glm::vec3 up = topDown ? glm::vec3(0,0,-1) : glm::vec3(0,1,0);
        glm::mat4 view=glm::lookAt(eye,target,up);
        glm::mat4 proj=glm::perspective(glm::radians(48.0f),float(WINDOW_W)/WINDOW_H,0.1f,100.0f);
        return proj*view;
    }
    void drawRoom(const glm::mat4& vp) {
        // Room shell: wood floor and wallpapered back/side walls.
        renderer.plane(shader,vp,{0,-0.08f,0},{14,14,1},{1,1,1},&wood,-90);
        renderer.plane(shader,vp,{0,4.0f,-7.0f},{14,8,1},{1,1,1},&wallpaper,0);
        renderer.plane(shader,vp,{-7.0f,4.0f,0},{14,8,1},{1,1,1},&wallpaper,0);
        // Three decor pieces: bookshelf, floor lamp, and potted plant.
        renderer.box(shader,vp,{-5.4f,1.25f,-5.9f},{1.5f,2.5f,0.7f},{0.30f,0.16f,0.09f});
        renderer.box(shader,vp,{-5.4f,1.1f,-5.48f},{1.35f,0.12f,0.12f},{0.68f,0.43f,0.22f});
        renderer.box(shader,vp,{-5.4f,1.9f,-5.48f},{1.35f,0.12f,0.12f},{0.68f,0.43f,0.22f});
        renderer.box(shader,vp,{5.5f,1.6f,-5.8f},{0.18f,3.2f,0.18f},{0.18f,0.17f,0.15f});
        renderer.box(shader,vp,{5.5f,3.25f,-5.8f},{0.9f,0.65f,0.9f},{0.88f,0.72f,0.38f});
        renderer.box(shader,vp,{5.2f,0.65f,-5.4f},{0.85f,1.3f,0.85f},{0.35f,0.19f,0.10f});
        renderer.box(shader,vp,{5.2f,1.55f,-5.4f},{1.2f,0.8f,1.2f},{0.12f,0.38f,0.20f});
        // Worn poster on the back wall.
        renderer.plane(shader,vp,{1.0f,2.8f,-6.92f},{2.0f,2.0f,1},{1,1,1},&poster,0);
    }
    void drawTable(const glm::mat4& vp) {
        renderer.box(shader,vp,{0,0.75f,0},{7.5f,0.35f,4.8f},{0.28f,0.13f,0.07f});
        renderer.plane(shader,vp,{0,0.94f,0},{7.35f,4.65f,1},{0.22f,0.34f,0.25f},nullptr,-90);
        // Stock pile: rectangular/quadrilateral card backs.
        for(int i=0;i<5;i++)
            renderer.plane(shader,vp,{-2.7f,1.15f+i*0.012f,0.0f},{0.95f,1.35f,1},{0.12f,0.18f,0.27f},nullptr,-90);
        // Player and opponent zones remain visually distinct.
        renderer.plane(shader,vp,{0.0f,1.155f,1.25f},{3.2f,1.55f,1},{0.12f,0.30f,0.18f},nullptr,-90);
        renderer.plane(shader,vp,{0.0f,1.155f,-1.35f},{3.2f,1.55f,1},{0.25f,0.20f,0.16f},nullptr,-90);
        // Cards in player's hand, spread in a readable row.
        const auto& hand=game.playerHand();
        float start=-0.5f*(float(hand.size())-1)*1.05f;
        for(std::size_t i=0;i<hand.size();++i){
            float x=start+float(i)*1.05f;
            renderer.plane(shader,vp,{x,1.19f,1.25f},{0.82f,1.22f,1},{1,1,1},&hand[i].texture,-90);
        }
    }
public:
    CardRoom(){
        wood.load("assets/wood.png"); wallpaper.load("assets/wallpaper.png"); poster.load("assets/torn_poster.png");
    }
    void render(){
        glClearColor(0.08f,0.09f,0.12f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        shader.use(); shader.integer("uTexture",0);
        glm::mat4 vp=cameraVP(); drawRoom(vp); drawTable(vp);
    }
    void key(int key){
        if(key==GLFW_KEY_1) game.setView(1);
        if(key==GLFW_KEY_2) game.setView(2);
        if(key==GLFW_KEY_G) game.drawCard();
    }
};

static CardRoom* app=nullptr;
void keyCallback(GLFWwindow* window,int key,int,int action,int){
    if(action==GLFW_PRESS && app) app->key(key);
}

int main(){
    if(!glfwInit()){ std::cerr<<"GLFW initialization failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window=glfwCreateWindow(WINDOW_W,WINDOW_H,"Card Room - Modern OpenGL",nullptr,nullptr);
    if(!window){ std::cerr<<"Window creation failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){ std::cerr<<"GLAD initialization failed\n"; return 1; }
    glEnable(GL_DEPTH_TEST);
    CardRoom game; app=&game; glfwSetKeyCallback(window,keyCallback);
    while(!glfwWindowShouldClose(window)){
        game.render(); glfwSwapBuffers(window); glfwPollEvents();
    }
    glfwTerminate(); return 0;
}
