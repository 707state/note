
// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <chrono>                  // for steady_clock
#include <cmath>                   // for cos
#include <vector>                  // for vector

#include "ftxui/component/component.hpp"       // for Renderer
#include "ftxui/component/loop.hpp"            // for Loop
#include "ftxui/component/app.hpp"             // for App
#include "ftxui/dom/canvas.hpp"                // for Canvas
#include "ftxui/dom/elements.hpp"              // for canvas, border, Element
#include "ftxui/screen/color.hpp"              // for Color

int main() {
  using namespace ftxui;

  // 用绝对时间推导相位：动画速度与帧率解耦，也不会累积浮点漂移
  const auto start = std::chrono::steady_clock::now();

  auto component = Renderer([&] {
    const auto now = std::chrono::steady_clock::now();
    const float phase =
        std::chrono::duration<float>(now - start).count() * 5.0f;

    auto c = Canvas(100, 100);

    // 坐标轴
    c.DrawPointLine(0, 50, 100, 50, Color::GrayLight);
    c.DrawPointLine(50, 0, 50, 100, Color::GrayLight);

    // 原有的三角形装饰
    c.DrawPointLine(10, 10, 30, 10, Color::Green);
    c.DrawPointLine(30, 10, 20, 30, Color::Green);
    c.DrawPointLine(20, 30, 10, 10, Color::Green);

    // 原有的圆装饰：空心圆 + 实心圆
    c.DrawPointCircle(80, 20, 10, Color::Blue);
    c.DrawPointCircleFilled(20, 80, 10, Color::Yellow);

    // 平移的 cos 曲线：phase 随时间递增 → 曲线在坐标轴上持续平移
    std::vector<int> ys(100);
    for (int x = 0; x < 100; x++) {
      ys[x] = int(50 - 20 * cos((x + phase) * 0.2f));
    }
    for (int x = 0; x < 99; x++) {
      c.DrawPointLine(x, ys[x], x + 1, ys[x + 1], Color::Red);
    }

    return canvas(std::move(c)) | border;
  });

  auto screen = App::FitComponent();
  Loop loop(&screen, component);

  // 单线程自定义循环：每帧请求重绘（App 内部以 60fps 上限驱动动画任务）
  while (!loop.HasQuitted()) {
    screen.RequestAnimationFrame();
    loop.RunOnce();
  }

  return 0;
}

