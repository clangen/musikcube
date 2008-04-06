#include "pch.hpp"
#include "Timer.hpp"

using namespace win32cpp;

Timer::Timer(unsigned int timeout) : timeout(timeout), wnd(NULL) {

    static unsigned int staticTimerCounter(0);

    ++staticTimerCounter;
    this->timerId   = staticTimerCounter;

}

Timer::~Timer(void){
}

void Timer::ConnectToWindow(win32cpp::Window *window){
    this->wnd   = window->Handle();
    window->TimerTimeout.connect(this,&Timer::OnTimerMsg);
}

bool Timer::Start(){
    if(SetTimer(this->wnd,this->timerId,this->timeout,NULL)==NULL)
        return false;

    return true;
}

bool Timer::Stop(){
    if(KillTimer(this->wnd,this->timerId)==NULL)
        return false;

    return true;
}

void Timer::OnTimerMsg(unsigned int timeoutId){
    if(this->timerId==timeoutId){
        this->OnTimout();
    }
}


