#pragma once

#include "BasePage.h"

#include "intro/Intro.h"

class IntroPage : public BasePage {
public:
    IntroPage(PageManager &manager, PageContext &context);

    virtual void draw(Canvas &canvas) override;

    virtual bool isModal() const override { return true; }
    // Close the modal intro page via user input (handled in IntroPage.cpp).
    virtual void keyPress(KeyPressEvent &event) override;

private:
    Intro _intro;

    uint32_t _lastTicks;
};
