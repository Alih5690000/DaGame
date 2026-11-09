#include <emscripten.h>
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#define EM_LOG(x) emscripten_log(1, x)

constexpr double RAD_TO_DEG = 180.0 / M_PI;

struct Vec2
{
    double x, y;

    Vec2 operator+(Vec2 b) const
    {
        return {x + b.x, y + b.y};
    }

    Vec2 operator-(Vec2 b) const
    {
        return {x - b.x, y - b.y};
    }

    Vec2 operator*(double s) const
    {
        return {x * s, y * s};
    }
};

double dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

struct Rect
{
    Vec2 center;
    double halfW, halfH;
    double angle; // Radians
};

Rect CreateRect(SDL_FRect r, double angle)
{
    return {
        {r.x + r.w / 2.0, r.y + r.h / 2.0},
        r.w / 2.0,
        r.h / 2.0,
        angle};
}

bool intersects(const Rect &a, const Rect &b)
{
    Vec2 uA = {
        std::cos(a.angle),
        std::sin(a.angle)};

    Vec2 vA = {
        -std::sin(a.angle),
        std::cos(a.angle)};

    Vec2 uB = {
        std::cos(b.angle),
        std::sin(b.angle)};

    Vec2 vB = {
        -std::sin(b.angle),
        std::cos(b.angle)};

    Vec2 axes[] = {uA, vA, uB, vB};
    Vec2 delta = b.center - a.center;

    for (Vec2 axis : axes)
    {
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

class Basic
{
public:
    SDL_FRect rect;
    bool active = true;
    bool collidable = false;
    int hp = 100;
    std::vector<Basic *> *sprites;
    Basic(std::vector<Basic *> &s)
    {
        sprites = &s;
    }
    virtual void takedmg(int hp)
    {
        this->hp -= hp;
    }
    virtual std::vector<Basic *> update(SDL_Renderer *, float dt) = 0;
    virtual ~Basic() = default;
    void HandleX(bool positive)
    {
        for (auto i : *sprites)
        {
            if (i->collidable && collidable && SDL_HasIntersectionF(&rect, &i->rect))
            {
                if (positive)
                {
                    rect.x = i->rect.x - i->rect.w;
                }
                else
                {
                    rect.x = i->rect.x + i->rect.w;
                }
            }
        }
    }
    void HandleX(int x)
    {
        bool p = x > 0;
        HandleX(p);
    }
    void HandleY(bool positive)
    {
        for (auto i : *sprites)
        {
            if (i->collidable && collidable && SDL_HasIntersectionF(&rect, &i->rect))
            {
                if (positive)
                {
                    rect.y = i->rect.y - i->rect.h;
                }
                else
                {
                    rect.y = i->rect.y + i->rect.h;
                }
            }
        }
    }
    void HandleY(int y)
    {
        bool p = y > 0;
        HandleY(p);
    }
    virtual void WhenParried(Basic*){
        
    }
};

class Bullet : public Basic
{
public:
    float dx, dy;
    float lifetime;
    Basic *shoo;
    Bullet(std::vector<Basic *> &s, float xx, float yy, SDL_FRect r, float l, Basic *shooter) : dx(xx), dy(yy), Basic(s), lifetime(l), shoo(shooter)
    {
        rect = r;
        collidable = false;
    }
    std::vector<Basic *> update(SDL_Renderer *r, float dt) override
    {
        rect.x += dx * dt;
        rect.y += dy * dt;
        lifetime -= dt;
        if (lifetime <= 0.f)
        {
            active = false;
            return {};
        }
        for (auto i : *sprites)
        {
            if (i==shoo || !i->collidable)
                continue;
            if (SDL_HasIntersectionF(&rect, &i->rect))
            {
                i->active = false;
                active = false;
            }
        }
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        SDL_RenderFillRectF(r, &rect);
        return {};
    }
};

class Player : public Basic
{
    void move(float dx, float dy)
    {
        std::vector<Basic *> moved;
        float offX = 0, offY = 0;
        for (auto i : *sprites)
        {
            if (i == this)
                continue;
            moved.push_back(i);
            i->rect.x -= dx;
            if (SDL_HasIntersectionF(&rect, &i->rect) && i->collidable)
            {
                if (dx > 0)
                {
                    offX = (rect.x + rect.w) - i->rect.x;
                }
                else if (dx < 0)
                {
                    offX = rect.x - (i->rect.x + i->rect.w);
                }
                goto end;
            }
            i->rect.y -= dy;
            if (SDL_HasIntersectionF(&rect, &i->rect) && i->collidable)
            {
                if (dy > 0)
                {
                    offY = (rect.y + rect.h) - i->rect.y;
                }
                else if (dy < 0)
                {
                    offY = rect.y - (i->rect.y + i->rect.h);
                }
                goto end;
            }
        }
    end:
        for (auto i : moved)
        {
            i->rect.x += offX;
            i->rect.y += offY;
        }
    }

public:
    Player(SDL_FRect r, std::vector<Basic *> &s) : Basic(s)
    {
        rect = r;
        collidable = true;
    }
    std::vector<Basic *> update(SDL_Renderer *r, float dt) override
    {
        std::vector<Basic *> res;
        int x, y;
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        Uint32 m = SDL_GetMouseState(&x, &y);
        if (m & SDL_BUTTON_LMASK)
        {
            float px = rect.x + rect.w / 2.0f;
            float py = rect.y + rect.h / 2.0f;

            float dx = x - px;
            float dy = y - py;

            float len = std::sqrt(dx * dx + dy * dy);

            if (len > 0.0f)
            {
                dx /= len;
                dy /= len;

                SDL_FRect bulletRect = {
                    px - 5.0f,
                    py - 5.0f,
                    10.0f,
                    10.0f};

                res.push_back(new Bullet(
                    *sprites,
                    dx * 600.0f,
                    dy * 600.0f,
                    bulletRect,
                    2.0f, this));
            }
        }
        if (k[SDL_SCANCODE_W])
        {
            move(0, -200 * dt);
        }
        if (k[SDL_SCANCODE_A])
        {
            move(-200 * dt, 0);
        }
        if (k[SDL_SCANCODE_S])
        {
            move(0, 200 * dt);
        }
        if (k[SDL_SCANCODE_D])
        {
            move(200 * dt, 0);
        }
        SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        SDL_RenderFillRectF(r, &rect);
        return res;
    }
    void takedmg(int hp) override
    {
        EM_LOG("Player took dmg");
        emscripten_cancel_main_loop();
    }
};

class Brick : public Basic
{
public:
    Brick(SDL_FRect r, std::vector<Basic *> &s) : Basic(s)
    {
        rect = r;
        collidable = true;
    }
    std::vector<Basic *> update(SDL_Renderer *r, float dt) override
    {
        SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
        SDL_RenderFillRectF(r, &rect);
        return {};
    }
};

class Beam : public Basic
{
public:
    float heatUp;
    float angle;
    float lifetime;
    float damagetime;
    bool timerOn = true;
    bool damaging = true;
    std::unordered_map<Basic *, float> guys;
    SDL_Texture *t;
    bool* f;
    Beam(float angle, std::vector<Basic *> &ss, float l, SDL_FRect r, SDL_Texture *tt, float h, float d, bool* f) : Basic(ss), lifetime(l)
    {
        rect = r;
        t = tt;
        this->f=f;
        this->angle = angle;
        heatUp = h;
        damagetime = d;
        collidable = false;
    }
    std::vector<Basic *> update(SDL_Renderer *r, float dt)
    {
        SDL_FPoint center = {0, rect.h / 2};
        if (heatUp > 0.f)
        {
            SDL_SetTextureAlphaMod(t, 128);
            SDL_RenderCopyExF(r, t, NULL, &rect,
                              angle * RAD_TO_DEG, &center, SDL_FLIP_NONE);
            heatUp -= dt;
            return {};
        }
        if (damaging)
        {
            if (damagetime <= 0.f)
                damaging = false;
            damagetime -= dt;
        }
        if (timerOn)
        {
            lifetime -= dt;
            if (lifetime <= 0.f)
            {
                active = false;
                return {};
            }
        }
        if (damaging)
            for (auto i : *sprites)
            {
                Rect ai = CreateRect(i->rect, 0);
                Vec2 dir = {std::cos(angle), std::sin(angle)};
                Rect me = {
                    {rect.x + dir.x * rect.w / 2.0,
                     rect.y + rect.h / 2.0 + dir.y * rect.w / 2.0},
                    rect.w / 2.0,
                    rect.h / 2.0,
                    angle};
                if (intersects(ai, me) && i != this &&
                    (guys.find(i) == guys.end() || guys[i] <= 0.f))
                {
                    EM_LOG("Hit");
                    i->takedmg(50);
                    guys[i] = 3.f;
                }
            }
        SDL_SetTextureAlphaMod(t, 255);
        SDL_RenderCopyExF(r, t, NULL, &rect,
                          angle * RAD_TO_DEG, &center, SDL_FLIP_NONE);
        return {};
    }
    ~Beam(){
        *f=true;
    }
};

class Turret : public Basic
{
public:
    Basic *t;
    const float CD = 5.f;
    const float DMG = 50;
    float cd = CD;
    float cool = 0.f;
    float lastAngle;
    SDL_FRect lastRect;
    SDL_Texture *txt;
    Beam *b;
    bool dead=false;
    Turret(Basic *target, std::vector<Basic *> &s, SDL_FRect r,
           SDL_Renderer *re) : t(target), Basic(s)
    {
        rect = r;
        txt = SDL_CreateTexture(re, SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
        SDL_SetTextureBlendMode(txt, SDL_BLENDMODE_BLEND);
        SDL_Texture *prev = SDL_GetRenderTarget(re);
        SDL_SetRenderTarget(re, txt);
        SDL_SetRenderDrawColor(re, 255, 255, 0, 255);
        SDL_RenderClear(re);
        SDL_SetRenderTarget(re, prev);
    }
    std::vector<Basic *> update(SDL_Renderer *r, float dt)
    {
        std::vector<Basic *> p;
        float dx = (t->rect.x + t->rect.w / 2.f) - (rect.x + rect.w / 2.f);
        float dy = (t->rect.y + t->rect.h / 2.f) - (rect.y + rect.h / 2.f);
        float angle = SDL_atan2f(dy, dx);
        float dist = SDL_sqrtf(dx * dx + dy * dy);
        SDL_FRect trail = {
            rect.x + rect.w / 2.f,
            rect.y + rect.h / 2.f - 5.f,
            dist,
            10.f};
        SDL_FPoint center = {0, trail.h / 2};
        cd -= dt;
        cool -= dt;
        if (cd > 0)
        {
            if (cool <= 0.f)
            {
                SDL_SetTextureAlphaMod(txt, 128);
                SDL_RenderCopyExF(r, txt, NULL, &trail,
                                  angle * RAD_TO_DEG, &center, SDL_FLIP_NONE);
            }
        }
        else
        {
            lastAngle = angle;
            lastRect = trail;
            dead=false;
            b = new Beam(lastAngle, *sprites, 1.f, lastRect, txt, 1.5f, 0.001f, &dead);
            p.push_back(b);
            cd = CD;
            cool = 2.5f;
        }
        return p;
    }
    ~Turret(){
        if (!dead)
            b->active=false;
    }
};

SDL_Renderer *renderer;
SDL_Window *window;

std::vector<Basic *> ss;
float dt;
int start, end;

void loop()
{
    start = SDL_GetTicks();
    dt = (start - end) / 1000.f;
    end = start;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_QUIT)
        {
            emscripten_cancel_main_loop();
        }
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    std::vector<Basic *> n;
    std::vector<Basic *> pending;
    for (auto i : ss)
    {
        auto k = i->update(renderer, dt);
        for (auto j : k)
        {
            pending.push_back(j);
        }
        if (i->active)
        {
            n.push_back(i);
        }
    }
    ss = n;
    for (auto i : pending)
    {
        ss.push_back(i);
    }
    SDL_RenderPresent(renderer);
}

void Init1(){
    ss.push_back(new Player({1000/2,800/2,100,100}, ss));
    ss.push_back(new Turret(ss[0], ss, {500,500,100,100},
        renderer));
    ss.push_back(new Brick({300,300,100,100}, ss));
}

int main(){
    SDL_Init(SDL_INIT_EVERYTHING);
    window=SDL_CreateWindow("Game", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1000, 800, SDL_WINDOW_SHOWN);
    renderer=SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED);
    Init1();
    emscripten_set_main_loop(loop, -1, 1);
}