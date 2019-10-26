#include "rack.hpp"
#include "InputScreen.h"
#include "XformScreens.h"
#include "InputScreenManager.h"


InputScreenManager::InputScreenManager(::rack::math::Vec siz) : size(siz)
{
}

InputScreenManager::~InputScreenManager()
{
    DEBUG("dtor iss");
    dismiss();
  // DEBUG("dtor iss 2");
}

void InputScreenManager::dismiss()
{
    DEBUG("In manager dismiis handler");

    auto tempScreen = screen;
    auto tempParent = parent;
    parent = nullptr;
    screen = nullptr;


    if (tempScreen) {
        auto values = tempScreen->getValues();
        DEBUG("values size = %d", values.size());
        for (auto x : values) {
            DEBUG("value = %.2f", x);
        }
        tempScreen->execute();
        tempScreen->clearChildren();
        this->callback();
        callback = nullptr;
    }
    if (tempParent) {
        tempParent->removeChild(tempScreen.get());
    }
}


template <class T>
std::shared_ptr<T> InputScreenManager::make(
    const ::rack::math::Vec& size,
    MidiSequencerPtr seq, 
    std::function<void(bool)> dismisser)
{
    return std::make_shared<T>(::rack::math::Vec(0, 0), size, seq, dismisser);
}

void InputScreenManager::show(
    ::rack::widget::Widget* parnt, 
    Screens screenId,  
    MidiSequencerPtr seq,
    Callback cb)
{

  // hard code to test screen for now

    this->callback = cb;
    parent = parnt;
    auto dismisser = [this](bool bOK) {
        DEBUG("in manager::dismisser %d", bOK);
        this->dismiss();
    };

    DEBUG("about to make input screen size = %f, %f", size.x, size.y);
    InputScreenPtr is;
    switch(screenId) {
        case Screens::Invert:
            is = make<XformInvert>(size, seq, dismisser);
            break;
        default:
            assert(false);
    }
    screen = is;
    parent->addChild(is.get());
    parentWidget = parent;
}