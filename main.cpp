#include <emscripten.h>
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>

constexpr double RAD_TO_DEG = 180.0 / M_PI;

struct Vec2 {
    double x, y;

    Vec2 operator+(Vec2 b) const {
        return {x + b.x, y + b.y};
    }

    Vec2 operator-(Vec2 b) const {
        return {x - b.x, y - b.y};
    }

    Vec2 operator*(double s) const {
        return {x * s, y * s};
    }
};

double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

struct Rect {
    Vec2 center;
    double halfW, halfH;
    double angle; // Radians
};

Rect CreateRect(SDL_FRect r, double angle){
    return {
        {r.x + r.w / 2.0, r.y + r.h / 2.0},
        r.w / 2.0,
        r.h / 2.0,
        angle
    };
}

bool intersects(const Rect& a, const Rect& b) {
    Vec2 uA = {
        std::cos(a.angle),
        std::sin(a.angle)
    };

    Vec2 vA = {
        -std::sin(a.angle),
        std::cos(a.angle)
    };

    Vec2 uB = {
        std::cos(b.angle),
        std::sin(b.angle)
    };

    Vec2 vB = {
        -std::sin(b.angle),
        std::cos(b.angle)
    };

    Vec2 axes[] = {uA, vA, uB, vB};
    Vec2 delta = b.center - a.center;

    for (Vec2 axis : axes) {
        double distance = std::abs(dot(delta, axis));

        double radiusA =
            a.halfW * std::abs(dot(uA, axis)) +
            a.halfH * std::abs(dot(vA, axis));

        double radiusB =
            b.halfW * std::abs(dot(uB, axis)) +
            b.halfH * std::abs(dot(vB, axis));

        if (distance > radiusA + radiusB)
            return false;
    }

    return true;
}

class Basic{
    public:
    SDL_FRect rect;
    int hp=100;
    std::vector<Basic*>* sprites;
    Basic(std::vector<Basic*>& s){
        sprites=&s;
    }
    virtual void update(SDL_Renderer*, float dt)=0;
};

class Player:public Basic{
    public:
    Player(SDL_FRect r, std::vector<Basic*>& s):
        Basic(s){
        rect=r;
    }
    void update(SDL_Renderer* r, float dt){
        const Uint8* k=SDL_GetKeyboardState(NULL);
        int x,y;
        Uint32 m=SDL_GetMouseState(&x, &y);
        rect.x=x;
        rect.y=y;
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        SDL_RenderFillRectF(r, &rect);
    }
};

class Turret:public Basic{
    public:
    Basic* t;
    const float CD=5.f;
    const float DMG=50;
    float cd=CD;
    float wait=0.f;
    bool waiting=false;
    bool AfterShot=false;
    float AfterShotCd=0.f;
    float lastAngle;
    SDL_FRect lastRect;
    SDL_Texture* txt;
    Turret(Basic* target,  std::vector<Basic*>& s, SDL_FRect r,
        SDL_Renderer* re):
        t(target),Basic(s){
        rect=r;
        txt=SDL_CreateTexture(re, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
        SDL_SetTextureBlendMode(txt, SDL_BLENDMODE_BLEND);
        SDL_Texture* prev=SDL_GetRenderTarget(re);
        SDL_SetRenderTarget(re, txt);
        SDL_SetRenderDrawColor(re, 255, 255, 0, 255);
        SDL_RenderClear(re);
        SDL_SetRenderTarget(re, prev);
    }
    void update(SDL_Renderer* r, float dt){
        float dx = (t->rect.x + t->rect.w / 2.f)
            - (rect.x + rect.w / 2.f);
        float dy = (t->rect.y + t->rect.h / 2.f)
            - (rect.y + rect.h / 2.f);
        float angle=SDL_atan2f(dy, dx)*RAD_TO_DEG;
        float dist=SDL_sqrtf(dx*dx+dy*dy);
        SDL_FRect trail={rect.x, rect.y, dist, 10};
        SDL_FPoint center={0, trail.h/2};
        cd-=dt;
        if (AfterShot){
            SDL_SetTextureAlphaMod(txt, 255);
            SDL_RenderCopyExF(r, txt, NULL, &lastRect,
                lastAngle, &center, SDL_FLIP_NONE);
            AfterShotCd-=dt;
            if (AfterShotCd<=0.f){
                cd=CD;
                AfterShot=false;
            }
            return;
        }
        if (waiting){
            if (wait>0.f){
                SDL_SetTextureAlphaMod(txt, 255);
                SDL_RenderCopyExF(r, txt, NULL, &lastRect,
                    lastAngle, &center, SDL_FLIP_NONE);
                wait-=dt;
            }
            else{
                SDL_SetTextureAlphaMod(txt, 255);
                SDL_RenderCopyExF(r, txt, NULL, &lastRect,
                    lastAngle, &center, SDL_FLIP_NONE);
                Rect shot=CreateRect(lastRect, lastAngle*RAD_TO_DEG);
                Rect target=CreateRect(t->rect, 0);
                if (intersects(shot, target)){
                    t->hp-=DMG;
                }
                AfterShot=true;
                AfterShotCd=1.f;
                waiting=false;
            }
            return;
        }
        if (cd>0){
            SDL_SetTextureAlphaMod(txt, 128);
            SDL_RenderCopyExF(r, txt, NULL, &trail,
                angle, &center, SDL_FLIP_NONE);
        }
        else{
            lastAngle=angle;
            lastRect=trail;
            waiting=true;
            wait=1.f;
        }
    }
};

SDL_Renderer* renderer;
SDL_Window* window;

std::vector<Basic*> ss;
float dt;
int start,end;

void loop(){
    start=SDL_GetTicks();
    dt=(start-end)/1000.f;
    end=start;
    SDL_Event e;
    while (SDL_PollEvent(&e)){
        if (e.type==SDL_QUIT){
            emscripten_cancel_main_loop();
        }
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    for (auto i:ss){
        i->update(renderer, dt);
    }
    SDL_RenderPresent(renderer);
}

int main(){
    SDL_Init(SDL_INIT_EVERYTHING);
    window=SDL_CreateWindow("Game", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1000, 800, SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED);
    ss.push_back(new Player({0,0,100,100}, ss));
    ss.push_back(new Turret(ss[0], ss, {500,500,100,100},
        renderer));
    emscripten_set_main_loop(loop, -1, 1);
}