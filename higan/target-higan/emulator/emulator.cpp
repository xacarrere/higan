#include "../higan.hpp"
#include "platform.cpp"
#include "video.cpp"
#include "audio.cpp"
#include "input.cpp"
#include "states.cpp"

Emulator emulator;

auto Emulator::create(shared_pointer<higan::Interface> instance, string location) -> void {
  interface = instance;

  system = {};
  system.name = Location::base(location).trimRight("/", 1L);
  system.data = location;
  system.templates = {Path::templates, interface->name(), "/"};

  string configuration = file::read({location, "settings.bml"});
  if(!configuration) {
    auto system = higan::Node::System::create();
    system->name = interface->name();
    system->setProperty("location", location);
    configuration = higan::Node::serialize(system);
  }

  //peripherals may have been renamed or deleted since last run; remove them from the configuration now
  auto document = BML::unserialize(configuration);
  for(auto node : document) validateConfiguration(node, document);
  configuration = BML::serialize(document);

  interface->load(configuration);
  root = interface->root();

  nodeManager.bind(root);
  systemMenu.setText(system.name);
  programWindow.show(home);
  programWindow.show(nodeManager);
  programWindow.setTitle(system.name);
  programWindow.setFocused();

  videoUpdate();
  audioUpdate();
  inputManager.bind(root);
  inputManager.poll();

  videoSettings.eventActivate();
  audioSettings.eventActivate();

  power(false);
}

auto Emulator::unload() -> void {
  if(!interface) return;

  power(false);
  programWindow.setTitle({"higan v", higan::Version});
  systemMenu.setText("System");

  if(auto location = root->property("location")) {
    file::write({location, "settings.bml"}, higan::Node::serialize(root));
  }

  inputManager.unbind();

  root = {};
  interface->unload();
  interface.reset();
}

auto Emulator::main() -> void {
  if(Application::modal()) return (void)usleep(20 * 1000);

  inputManager.poll();
  hotkeys.poll();

  if(!interface) return (void)usleep(20 * 1000);

  if(!system.power || (!programWindow.viewport.focused() && settings.input.unfocused == "Pause")) {
    usleep(20 * 1000);
  } else {
    interface->run();
  }
}

auto Emulator::quit() -> void {
  Application::quit();  //stop processing callbacks and timers
  unload();

  interfaces.reset();

  videoInstance.reset();
  audioInstance.reset();
  inputInstance.reset();
}

auto Emulator::power(bool on) -> void {
  if(system.power == on) return;
  if(system.power = on) {
    programWindow.setTitle(interface->game());
    videoUpdateColors();
    audioUpdateEffects();
    interface->power();
    //powering on the system latches static settings
    nodeManager.refreshSettings();
    if(settingEditor.visible()) settingEditor.refresh();
    programWindow.viewport.setFocused();
  } else {
    programWindow.setTitle(system.name);
    videoInstance.clear();
    audioInstance.clear();
  }
  systemMenu.power.setChecked(on);
  toolsMenu.saveStateMenu.setEnabled(on);
  toolsMenu.loadStateMenu.setEnabled(on);
}

//used to prevent connecting the same (emulated) physical device to multiple ports simultaneously
auto Emulator::connected(string location) -> higan::Node::Port {
  for(auto& peripheral : root->find<higan::Node::Peripheral>()) {
    if(location == peripheral->property("location")) return peripheral->parent.acquire();
  }
  return {};
}

auto Emulator::validateConfiguration(Markup::Node node, Markup::Node parent) -> void {
  for(auto property : node.find("property")) {
    if(property["name"].text() != "location") continue;
    auto location = property["value"].text();
    //if the peripheral is missing, remove it from the tree
    if(!directory::exists(location)) parent.remove(node);
  }
  for(auto branch : node) validateConfiguration(branch, node);
}