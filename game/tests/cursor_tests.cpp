#include "game/platform/idle_cursor.h"
#include "game/development/development_overlay.h"
#include "engine/tools/development_overlay_host.h"
#include "engine/core/time.h"
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <memory>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
using game::platform::CursorSample;
using game::platform::IdleCursor;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
CursorSample sample;
CursorSample injected_sample(SDL_Window*){return sample;}
void check_policy(SDL_Window* window){
    IdleCursor cursor(injected_sample);sample={0,50,60,true};cursor.start(window);
    require(SDL_CursorVisible()&&!cursor.hidden(),"Starting the cursor controller must show its cursor");
    sample.now=1'999'999'999;cursor.update();require(SDL_CursorVisible(),"Cursor must remain visible before two seconds");
    sample.now=2'000'000'000;cursor.update();require(!SDL_CursorVisible()&&IdleCursor::hides_cursor(),"Cursor must hide at the exact two-second boundary");
    sample.x+=.25f;cursor.update();require(SDL_CursorVisible(),"Even a small actual mouse movement must reveal the cursor");
    sample.now+=1'999'999'999;cursor.update();require(SDL_CursorVisible(),"Movement must restart the full delay");
    ELYSIA_TIME->set_time_scale(0);ELYSIA_TIME->begin_frame(10);
    ++sample.now;cursor.update();require(cursor.hidden(),"Zero simulation time scale must not stop the idle deadline");
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
    require(SDL_PushEvent(&event),"Held mouse input fixture failed");
    event={};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_SPACE;event.key.scancode=SDL_SCANCODE_SPACE;event.key.down=true;
    require(SDL_PushEvent(&event),"Keyboard input fixture failed");SDL_PumpEvents();
    sample.now+=10'000'000'000;cursor.update();require(cursor.hidden(),"Stationary held mouse/keyboard input must not reveal or reset the cursor");
    sample.focused=false;cursor.update();require(SDL_CursorVisible(),"Focus loss or mouse leave must restore the cursor");
    sample.now+=10'000'000'000;cursor.update();require(SDL_CursorVisible(),"Inactive windows must never hide the desktop cursor");
    sample.focused=true;cursor.update();require(SDL_CursorVisible(),"Focus regain must reveal and restart the timer");
    sample.now+=2'000'000'000;cursor.update();require(cursor.hidden(),"Focus regain must receive a new two-second deadline");
    cursor.stop();require(SDL_CursorVisible()&&!IdleCursor::hides_cursor(),"Stopping the cursor controller must restore the cursor");
    cursor.start(window);require(SDL_CursorVisible(),"Restarting the controller must start visible without a stale deadline");
    sample.now+=2'000'000'000;cursor.update();require(cursor.hidden(),"A restarted controller must independently become idle");
    {
        IdleCursor replacement(injected_sample);replacement.start(window);
        require(SDL_CursorVisible(),"Replacing the controller must restore cursor visibility");
        sample.now+=2'000'000'000;replacement.update();cursor.stop();
        require(replacement.hidden()&&!SDL_CursorVisible(),"An old controller must not override its replacement's cursor ownership");
    }
    require(SDL_CursorVisible()&&!IdleCursor::hides_cursor(),"Destruction must restore the cursor even without an explicit stop");
    ELYSIA_TIME->reset();
}
void pump(){SDL_Event event;while(SDL_PollEvent(&event)){};}
void check_global_application(SDL_Window* window){
    const std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(SDL_CreateRenderer(window,"software"),SDL_DestroyRenderer);
    require(renderer!=nullptr,SDL_GetError());
    sample={0,20,30,true};elysia::tools::DevelopmentOverlayHost host;
    host.set_overlay(std::make_unique<game::development::DevelopmentOverlay>(injected_sample));
    require(host.initialize(*window,*renderer).has_value(),"Global cursor runtime initialization failed");
    const auto frame=[&]{host.begin_frame(0);host.render(*renderer);};
    require(!host.visible()&&SDL_CursorVisible(),"Application startup must show the cursor without developer UI");
    sample.now=2'000'000'000;frame();require(!SDL_CursorVisible(),"Global cursor sampling must run while all developer UI is hidden");
    sample.now+=1'000'000'000;frame();require(!SDL_CursorVisible(),"Page-independent frames must retain the global idle state");
    ++sample.x;frame();require(SDL_CursorVisible(),"Global mouse motion must reveal the cursor with zero simulation delta");
    sample.focused=false;sample.now+=5'000'000'000;frame();require(SDL_CursorVisible(),"Application focus loss must restore the desktop cursor");
    sample.focused=true;frame();sample.now+=2'000'000'000;frame();require(!SDL_CursorVisible(),"Global idle hiding must resume after focus returns");
    require(host.captured_input()==elysia::input::InputCapture::None,"Cursor-only background processing must not capture gameplay input");
    host.shutdown();require(SDL_CursorVisible(),"Application shutdown must restore the cursor");
}
bool platform_cursor_visible(){
#ifdef _WIN32
    CURSORINFO info{sizeof(CURSORINFO)};
    require(GetCursorInfo(&info)!=0,"Windows cursor visibility query failed");
    // SDL hides via SetCursor(NULL), which need not change ShowCursor's count.
    return (info.flags&CURSOR_SHOWING)!=0&&info.hCursor!=nullptr;
#else
    return SDL_CursorVisible();
#endif
}
void check_native_window(SDL_Window* window){
    SDL_RaiseWindow(window);SDL_WarpMouseInWindow(window,100,100);
    SDL_SetWindowTitle(window,"FlyMe cursor test - click to start");
    bool clicked=false;const auto click_deadline=SDL_GetTicks()+45000;
    while(!clicked&&SDL_GetTicks()<click_deadline){
        SDL_Event event;while(SDL_PollEvent(&event))if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.windowID==SDL_GetWindowID(window))clicked=true;
        SDL_Delay(10);
    }
    require(clicked,"Native window test needs a real click inside its window to establish physical focus");
    const auto focus_deadline=SDL_GetTicks()+20000;
    while((SDL_GetMouseFocus()!=window||SDL_GetKeyboardFocus()!=window)&&SDL_GetTicks()<focus_deadline){pump();SDL_Delay(10);}
    require(SDL_GetMouseFocus()==window&&SDL_GetKeyboardFocus()==window,"Native test window could not gain mouse and keyboard focus");
    float initial_x=0,initial_y=0;SDL_GetGlobalMouseState(&initial_x,&initial_y);
    IdleCursor cursor;cursor.start(window);require(platform_cursor_visible(),"Native cursor must initially be visible");
    const auto visible_after_pump=[&](Uint64 wait_ms=250){
        // Native WM_SETCURSOR delivery can follow the warp's mouse-motion event.
        const auto until=SDL_GetTicks()+wait_ms;
        do{pump();cursor.update();if(!cursor.hidden()&&SDL_CursorVisible()&&platform_cursor_visible())return true;SDL_Delay(10);}while(SDL_GetTicks()<until);
        return false;
    };
    ELYSIA_TIME->set_time_scale(0);
    const auto deadline=SDL_GetTicks()+3000;
    while(!cursor.hidden()&&SDL_GetTicks()<deadline){pump();cursor.update();SDL_Delay(10);}
    require(cursor.hidden()&&!SDL_CursorVisible()&&!platform_cursor_visible(),"Native cursor must actually disappear while simulation time is paused");
    SDL_WarpMouseInWindow(window,180,140);pump();cursor.update();
    SDL_SetWindowTitle(window,"FlyMe cursor test - move mouse inside");
    const bool moved_visible=visible_after_pump(45000);
    if(!moved_visible){
        float x=0,y=0,lx=0,ly=0;SDL_GetGlobalMouseState(&x,&y);SDL_GetMouseState(&lx,&ly);
        std::cerr<<"motion diagnostic: hidden="<<cursor.hidden()<<" SDL-visible="<<SDL_CursorVisible()<<" OS-visible="<<platform_cursor_visible()<<" initial="<<initial_x<<","<<initial_y<<" global="<<x<<","<<y<<" local="<<lx<<","<<ly<<" mouse-focus="<<(SDL_GetMouseFocus()==window)<<" keyboard-focus="<<(SDL_GetKeyboardFocus()==window)<<"\n";
#ifdef _WIN32
        CURSORINFO info{sizeof(CURSORINFO)};GetCursorInfo(&info);std::cerr<<"Windows cursor flags="<<info.flags<<" handle="<<info.hCursor<<" thread-handle="<<GetCursor()<<"\n";
#endif
    }
    require(moved_visible,"Physical cursor motion must restore the native cursor");
    const auto again=SDL_GetTicks()+3000;
    while(!cursor.hidden()&&SDL_GetTicks()<again){pump();cursor.update();SDL_Delay(10);}
    require(cursor.hidden(),"Native cursor must hide again after a fresh interval");
    require(SDL_CaptureMouse(true),"Native outside-window capture fixture failed");
    SDL_WarpMouseInWindow(window,-30,100);pump();cursor.update();
    const auto outside=SDL_GetTicks()+2100;
    while(SDL_GetTicks()<outside){pump();cursor.update();SDL_Delay(10);}
    require(SDL_CursorVisible()&&platform_cursor_visible(),"Mouse capture must not hide the desktop cursor outside the client area");
    SDL_CaptureMouse(false);SDL_WarpMouseInWindow(window,180,140);pump();cursor.update();
    const auto restored=SDL_GetTicks()+3000;
    while(!cursor.hidden()&&SDL_GetTicks()<restored){pump();cursor.update();SDL_Delay(10);}
    require(cursor.hidden(),"Returning inside must restart the native cursor timer");
    SDL_MinimizeWindow(window);
    const auto minimized=SDL_GetTicks()+1500;
    while(!(SDL_GetWindowFlags(window)&SDL_WINDOW_MINIMIZED)&&SDL_GetTicks()<minimized){pump();SDL_Delay(10);}
    require(visible_after_pump(),"Minimizing the native window must restore the desktop cursor");
    cursor.stop();ELYSIA_TIME->reset();
}
}
int main(int argc,char** argv){
    const bool native=argc==2&&std::string_view(argv[1])=="--window-smoke";
    SDL_Window* window=nullptr;int result=1;
    try{
        if(!native)SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        window=SDL_CreateWindow("FlyMe cursor visibility test",640,360,native?0:SDL_WINDOW_HIDDEN);
        require(window!=nullptr,SDL_GetError());
        if(native)check_native_window(window);else {check_policy(window);check_global_application(window);}
        std::cout<<(native?"PASS: native Windows/SDL cursor idle, real movement, zero time scale and minimize restoration\n":"PASS: two-second boundary, movement, held input, focus, retry and cursor ownership\n");result=0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';}
    SDL_ShowCursor();SDL_DestroyWindow(window);SDL_Quit();return result;
}
