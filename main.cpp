#include <emscripten.h>
#include <SDL2/SDL.h>
#include <vector>

class Basic{
    public:
    SDL_FRect rect;
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
    emscripten_set_main_loop(loop, -1, 1);
}