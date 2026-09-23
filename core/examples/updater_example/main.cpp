#include <iostream>

#include "dazzle.h"

using dazzle::Updater;

int main() {
  auto& updater = Updater::GetInstance();

  std::string initial = updater.GetFeedUrl();
  std::cout << "Initial feed URL: " << (initial.empty() ? "(none)" : initial) << std::endl;

  updater.SetFeedUrl("https://example.com/appcast.xml");
  std::cout << "Feed URL:         " << updater.GetFeedUrl() << std::endl;

  return 0;
}
