#include <emscripten.h>
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#define EM_LOG(x) emscripten_log(1, x)

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
    bool active=true;
    int hp=100;
    std::vector<Basic*>* sprites;
    Basic(std::vector<Basic*>& s){
        sprites=&s;
    }
    virtual void takedmg(int hp){
        this->hp-=hp;
    }
    virtual std::vector<Basic*> update(SDL_Renderer*, float dt)=0;
    virtual ~Basic()=default;
};

class Player:public Basic{
    void move(float dx, float dy){
        for (auto i:*sprites){
            if (i==this) continue;
            i->rect.x-=dx;
            i->rect.y-=dy;
        }
    }
    public:
    Player(SDL_FRect r, std::vector<Basic*>& s):
        Basic(s){
        rect=r;
    }
    std::vector<Basic*> update(SDL_Renderer* r, float dt) override{
        int x,y;
        const Uint8* k=SDL_GetKeyboardState(NULL);
        if (k[SDL_SCANCODE_W]){
            move(0,-200*dt);
        }
        if (k[SDL_SCANCODE_A]){
            move(-200*dt, 0);
        }
        if (k[SDL_SCANCODE_S]){
            move(0,200*dt);
        }
        if (k[SDL_SCANCODE_D]){
            move(200*dt, 0);
        }
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        SDL_RenderFillRectF(r, &rect);
        return {};
    }
    void takedmg(int hp) override{
        EM_LOG("Player took dmg");
        emscripten_cancel_main_loop();
    }
};

class Beam:public Basic{
    public:
    float heatUp;
    float angle;
    float lifetime;
    float damagetime;
    bool timerOn=true;
    bool damaging=true;
    std::unordered_map<Basic*,float> guys;
    SDL_Texture* t;
    Beam(float angle, std::vector<Basic*>& ss, float l, SDL_FRect r, SDL_Texture* tt, float h
        , float d):Basic(ss),lifetime(l){
        rect=r;
        t=tt;
        this->angle = angle;
        heatUp=h;
        damagetime=d;
    }
    std::vector<Basic*> update(SDL_Renderer* r, float dt){
        SDL_FPoint center={0, rect.h/2};
        if (heatUp>0.f){
            SDL_SetTextureAlphaMod(t, 128);
            SDL_RenderCopyExF(r, t, NULL, &rect,
                angle*RAD_TO_DEG, &center, SDL_FLIP_NONE);
            heatUp-=dt;
            return {};
        }
        if (damaging){
            damagetime-=dt;
            if (damagetime<=0.f) damaging=false;
        }
        if (timerOn){
            lifetime-=dt;
            if (lifetime<=0.f){
                active=false;
                return {};
            }
        }
        if (damaging)
            for (auto i:*sprites){
                Rect ai=CreateRect(i->rect, 0);
                Vec2 dir = {std::cos(angle), std::sin(angle)};
                Rect me = {
                    {
                        rect.x + dir.x * rect.w / 2.0,
                        rect.y + rect.h / 2.0 + dir.y * rect.w / 2.0
                    },
                    rect.w / 2.0,
                    rect.h / 2.0,
                    angle
                };
                if (intersects(ai, me) && i!=this &&
                    (guys.find(i)==guys.end() || guys[i]<=0.f)){
                    EM_LOG("Hit");
                    i->takedmg(50);
                    guys[i]=3.f;
                }
            }
        SDL_SetTextureAlphaMod(t, 255);
        SDL_RenderCopyExF(r, t, NULL, &rect,
            angle*RAD_TO_DEG, &center, SDL_FLIP_NONE);
        return {};
    }
};

class Turret:public Basic{
    public:
    Basic* t;
    const float CD=5.f;
    const float DMG=50;
    float cd=CD;
    float cool=0.f;
    float lastAngle;
    SDL_FRect lastRect;
    SDL_Texture* txt;
    Beam* b;
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
    std::vector<Basic*> update(SDL_Renderer* r, float dt){
        std::vector<Basic*> p;
        float dx = (t->rect.x + t->rect.w / 2.f)
            - (rect.x + rect.w / 2.f);
        float dy = (t->rect.y + t->rect.h / 2.f)
            - (rect.y + rect.h / 2.f);
        float angle=SDL_atan2f(dy, dx);
        float dist=SDL_sqrtf(dx*dx+dy*dy);
        SDL_FRect trail = {
            rect.x + rect.w / 2.f,
            rect.y + rect.h / 2.f - 5.f,
            dist,
            10.f
        };
        SDL_FPoint center={0, trail.h/2};
        cd-=dt;
        cool-=dt;
        if (cd>0){
            if (cool<=0.f){
                SDL_SetTextureAlphaMod(txt, 128);
                SDL_RenderCopyExF(r, txt, NULL, &trail,
                    angle*RAD_TO_DEG, &center, SDL_FLIP_NONE);
            }
        }
        else{
            lastAngle=angle;
            lastRect=trail;
            b=new Beam(lastAngle, *sprites, 1.f, lastRect, txt, 1.5f, 0.001f);
            p.push_back(b);
            cd=CD;
            cool=2.5f;
        }
        return p;
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
    std::vector<Basic*> n;
    std::vector<Basic*> pending;
    for (auto i:ss){
        auto k=i->update(renderer, dt);
        for (auto j:k){
            pending.push_back(j);
        }
        if (i->active){
            n.push_back(i);
        }
    }
    ss=n;
    for (auto i:pending){
        ss.push_back(i);
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
    ss.push_back(new Player({1000/2,800/2,100,100}, ss));
    ss.push_back(new Turret(ss[0], ss, {500,500,100,100},
        renderer));
    emscripten_set_main_loop(loop, -1, 1);
}