#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

int main() {
  // State
  int counter = 0;
  int slider_value = 50;
  bool checked = false;
  std::vector<std::string> menu_entries = {"Option A", "Option B", "Option C"};
  int menu_selected = 0;

  // Components
  auto slider = Slider("Slider:", &slider_value, 0, 100, 1);

  auto checkbox = Checkbox("Enable feature", &checked);

  auto menu = Menu(&menu_entries, &menu_selected);

  auto button_dec = Button("Decrement", [&] { --counter; });
  auto button_inc = Button("Increment", [&] { ++counter; });
  auto button_reset = Button("Reset", [&] { counter = 0; });

  auto buttons = Container::Horizontal({button_dec, button_inc, button_reset});

  auto left_panel = Container::Vertical({menu, checkbox});

  auto right_panel = Container::Vertical({
      buttons,
      slider,
  });

  auto main_container = Container::Horizontal({left_panel, right_panel});

  // Renderer
  auto renderer = Renderer(main_container, [&] {
    return vbox({
        text(" FTXUI Demo ") | bold | center | border,
        separator(),
        hbox({
            vbox({
                text(" Menu ") | bold,
                menu->Render() | border | size(WIDTH, GREATER_THAN, 20),
                separator(),
                text(" Toggle ") | bold,
                checkbox->Render(),
            }) | flex,
            separator(),
            vbox({
                text(" Counter ") | bold,
                hbox({
                    text("  Value: "),
                    text(std::to_string(counter)) | bold | color(Color::Cyan),
                }),
                buttons->Render(),
                separator(),
                text(" Slider ") | bold,
                slider->Render() | size(WIDTH, GREATER_THAN, 30),
            }) | flex,
        }) | flex,
        separator(),
        hbox({
            text(" Menu: ") | dim,
            text(menu_entries[menu_selected]) | color(Color::Yellow),
            filler(),
            text(" Slider: ") | dim,
            text(std::to_string(slider_value) + "%") | color(Color::Green),
            filler(),
            text(" Checkbox: ") | dim,
            text(checked ? "ON" : "OFF") | color(checked ? Color::Green : Color::Red),
            filler(),
            text(" Ctrl+C to quit ") | dim | center,
        }),
    }) |
    border | center;
  });

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(renderer);

  return 0;
}
