
#include "TTauri/Widgets/widgets.hpp"
#include "TTauri/GUI/Window.hpp"
#include "TTauri/GUI/WindowDelegate.hpp"
#include "TTauri/GUI/WindowToolbarWidget.hpp"
#include "TTauri/GUI/Instance.hpp"
#include "TTauri/Audio/globals.hpp"
#include "TTauri/Audio/AudioSystem.hpp"
#include "TTauri/Application/Application.hpp"

#include <vulkan/vulkan.hpp>

#include <Windows.h>

#include <memory>
#include <vector>

using namespace std;
using namespace TTauri;
using namespace TTauri::Audio;
using namespace TTauri;
using namespace TTauri::GUI;
using namespace TTauri::GUI::Widgets;

class MyWindowDelegate : public WindowDelegate {
public:
    void openingWindow(Window &window) override
    {
        auto button1 = window.widget->addWidget<ButtonWidget>(u8"H\u00eb""ll\u00f6 W\u00f6""rld");
        window.addConstraint(button1->box.width == 100);
        window.addConstraint(button1->box.height == 30);
        window.addConstraint(button1->box.outerLeft() == window.widget->box.left);
        window.addConstraint(button1->box.outerBottom() == window.widget->box.bottom);
        window.addConstraint(button1->box.outerTop() <= window.widget->toolbar->box.bottom);

        auto button2 = window.widget->addWidget<ButtonWidget>(u8"Foo Bar");
        window.addConstraint(button2->box.width >= 100);
        window.addConstraint(button2->box.width <= 1500);
        window.addConstraint(button2->box.height == 30);
        window.addConstraint(button2->box.outerLeft() == button1->box.right());
        window.addConstraint(button2->box.outerBottom() == window.widget->box.bottom);
        window.addConstraint(button2->box.outerRight() == window.widget->box.right());
        window.addConstraint(button2->box.outerTop() <= window.widget->toolbar->box.bottom);
    }

    void closingWindow(const Window &window) override
    {
        LOG_INFO("Window being destroyed.");
    }
};

class MyApplicationDelegate : public ApplicationDelegate {
public:
    std::string applicationName() const noexcept override {
        return "TTauri Development Application";
    }

    std::vector<TTauri::OptionConfig> optionConfig() const noexcept override {
        return {};
    }

    bool startingLoop() override
    {
        auto myWindowDelegate = make_shared<MyWindowDelegate>();

        GUI_globals->instance().initialize();
        GUI_globals->instance().addWindow<Window>(myWindowDelegate, "Hello World 1");

        Audio_globals->audioSystem().initialize();
        return true;
    }

    void lastWindowClosed() override
    {
    }

    void audioDeviceListChanged() override
    {
        LOG_INFO("MyApplicationDelegate::audioDeviceListChanged()");
    }
};

MAIN_DEFINITION
{
    auto myApplicationDelegate = make_shared<MyApplicationDelegate>();

    auto app = Application(myApplicationDelegate, MAIN_ARGUMENTS);

    return app.loop();
}
