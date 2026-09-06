#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "ftxui/dom/canvas.hpp"

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Vec3 {
  double x;
  double y;
  double z;
};

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 v, double scalar) {
  return {v.x * scalar, v.y * scalar, v.z * scalar};
}

double Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

double Length(Vec3 v) { return std::sqrt(Dot(v, v)); }

Vec3 Normalize(Vec3 v) {
  const double length = Length(v);
  return length > 1e-12 ? v * (1.0 / length) : Vec3{0.0, 0.0, 0.0};
}

struct Quaternion {
  double w;
  double x;
  double y;
  double z;
};

Quaternion operator+(Quaternion a, Quaternion b) {
  return {a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z};
}

Quaternion operator*(Quaternion q, double scalar) {
  return {q.w * scalar, q.x * scalar, q.y * scalar, q.z * scalar};
}

Quaternion operator-(Quaternion q) { return q * -1.0; }

double Dot(Quaternion a, Quaternion b) {
  return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
}

double Length(Quaternion q) { return std::sqrt(Dot(q, q)); }

Quaternion Normalize(Quaternion q) {
  const double length = Length(q);
  return length > 1e-12 ? q * (1.0 / length) : Quaternion{1.0, 0.0, 0.0, 0.0};
}

Quaternion Conjugate(Quaternion q) { return {q.w, -q.x, -q.y, -q.z}; }

Quaternion operator*(Quaternion a, Quaternion b) {
  return {
      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
  };
}

Vec3 Rotate(Quaternion q, Vec3 v) {
  const Quaternion vector_quaternion{0.0, v.x, v.y, v.z};
  const Quaternion rotated = q * vector_quaternion * Conjugate(q);
  return {rotated.x, rotated.y, rotated.z};
}

// Spherical linear interpolation on the unit-quaternion sphere.
// Negating q1 when the dot product is negative selects the shortest arc.
Quaternion Slerp(Quaternion q0, Quaternion q1, double t) {
  q0 = Normalize(q0);
  q1 = Normalize(q1);
  t = std::clamp(t, 0.0, 1.0);

  double cosine = Dot(q0, q1);
  if (cosine < 0.0) {
    q1 = -q1;
    cosine = -cosine;
  }

  // For a very small angle, normalized linear interpolation is more stable
  // than dividing by sin(theta), which is close to zero.
  if (cosine > 0.9995) {
    return Normalize(q0 * (1.0 - t) + q1 * t);
  }

  cosine = std::clamp(cosine, -1.0, 1.0);
  const double angle = std::acos(cosine);
  const double sine = std::sin(angle);
  const double weight0 = std::sin((1.0 - t) * angle) / sine;
  const double weight1 = std::sin(t * angle) / sine;
  return Normalize(q0 * weight0 + q1 * weight1);
}

struct ProjectedPoint {
  int x;
  int y;
};

// A fixed oblique camera makes all three axes visible in the terminal.
ProjectedPoint Project(Vec3 point, int width, int height) {
  constexpr double kYaw = -0.65;
  constexpr double kPitch = 0.42;
  // Canvas coordinates are braille pixels. A larger scale makes the
  // orientation axes and interpolation arc easy to distinguish in a terminal.
  constexpr double kScale = 20.0;

  const double yaw_x = point.x * std::cos(kYaw) - point.z * std::sin(kYaw);
  const double yaw_z = point.x * std::sin(kYaw) + point.z * std::cos(kYaw);
  const double screen_y = point.y * std::cos(kPitch) - yaw_z * std::sin(kPitch);

  return {static_cast<int>(width * 0.5 + yaw_x * kScale),
          static_cast<int>(height * 0.5 - screen_y * kScale)};
}

void DrawLine(ftxui::Canvas& canvas,
              Vec3 from,
              Vec3 to,
              const ftxui::Color& color) {
  const auto start = Project(from, canvas.width(), canvas.height());
  const auto end = Project(to, canvas.width(), canvas.height());
  canvas.DrawPointLine(start.x, start.y, end.x, end.y, color);
}

void DrawPoint(ftxui::Canvas& canvas,
               Vec3 point,
               int radius,
               const ftxui::Color& color) {
  const auto projected = Project(point, canvas.width(), canvas.height());
  canvas.DrawPointCircleFilled(projected.x, projected.y, radius, color);
}

void DrawFrame(ftxui::Canvas& canvas, double t) {
  const Quaternion q0{1.0, 0.0, 0.0, 0.0};
  const Quaternion q1{std::cos(kPi * 0.375),
                      std::sin(kPi * 0.375) * 0.8,
                      std::sin(kPi * 0.375) * 0.45,
                      std::sin(kPi * 0.375) * 0.4};
  const Quaternion current = Slerp(q0, q1, t);

  const Vec3 origin{0.0, 0.0, 0.0};
  const Vec3 x_axis{1.0, 0.0, 0.0};
  const Vec3 y_axis{0.0, 1.0, 0.0};
  const Vec3 z_axis{0.0, 0.0, 1.0};

  // The interpolated tip traces a spherical arc. Draw it first so the
  // current orientation remains visually prominent.
  Vec3 previous = Rotate(q0, z_axis) * 1.7;
  for (int i = 1; i <= 80; ++i) {
    const double sample_t = static_cast<double>(i) / 80.0;
    const Vec3 next = Rotate(Slerp(q0, q1, sample_t), z_axis) * 1.7;
    DrawLine(canvas, previous, next, ftxui::Color::RGB(80, 80, 100));
    previous = next;
  }

  // Reference orientation and target orientation.
  DrawLine(canvas, origin, Rotate(q0, x_axis), ftxui::Color::RGB(100, 45, 45));
  DrawLine(canvas, origin, Rotate(q0, y_axis), ftxui::Color::RGB(45, 100, 45));
  DrawLine(canvas, origin, Rotate(q0, z_axis), ftxui::Color::RGB(45, 45, 120));
  DrawLine(canvas, origin, Rotate(q1, x_axis), ftxui::Color::RGB(130, 60, 60));
  DrawLine(canvas, origin, Rotate(q1, y_axis), ftxui::Color::RGB(60, 130, 60));
  DrawLine(canvas, origin, Rotate(q1, z_axis), ftxui::Color::RGB(60, 60, 150));

  // Current orientation: RGB corresponds to the local X/Y/Z axes.
  DrawLine(canvas, origin, Rotate(current, x_axis), ftxui::Color::Red);
  DrawLine(canvas, origin, Rotate(current, y_axis), ftxui::Color::Green);
  DrawLine(canvas, origin, Rotate(current, z_axis), ftxui::Color::Cyan);
  DrawPoint(canvas, Rotate(current, z_axis) * 1.7, 3, ftxui::Color::Yellow);
  DrawPoint(canvas, origin, 3, ftxui::Color::White);
}

std::string Format(double value) {
  std::ostringstream output;
  output << std::fixed << std::setprecision(3) << value;
  return output.str();
}

std::string QuaternionText(const char* name, Quaternion q) {
  return std::string(name) + " = (" + Format(q.w) + ", " + Format(q.x) + ", " +
         Format(q.y) + ", " + Format(q.z) + ")";
}

}  // namespace

int main() {
  using namespace ftxui;

  auto screen = App::FitComponent();
  bool paused = false;
  double animation_elapsed = 0.0;
  auto last_tick = std::chrono::steady_clock::now();

  auto renderer = Renderer([&] {
    const auto now = std::chrono::steady_clock::now();
    const double delta =
        std::chrono::duration<double>(now - last_tick).count();
    last_tick = now;
    if (!paused) {
      animation_elapsed += delta;
    }

    // Ping-pong t demonstrates both directions without a discontinuous jump.
    // animation_elapsed stops changing while paused, so the current frame is
    // kept exactly where it was when the space key was pressed.
    const double cycle = std::fmod(animation_elapsed, 6.0) / 6.0;
    const double t = cycle <= 0.5 ? cycle * 2.0 : (1.0 - cycle) * 2.0;

    const Quaternion q0{1.0, 0.0, 0.0, 0.0};
    const Quaternion q1{std::cos(kPi * 0.375),
                        std::sin(kPi * 0.375) * 0.8,
                        std::sin(kPi * 0.375) * 0.45,
                        std::sin(kPi * 0.375) * 0.4};
    const Quaternion current = Slerp(q0, q1, t);

    screen.RequestAnimationFrame();
    auto scene = canvas(140, 72, [t](Canvas& canvas) { DrawFrame(canvas, t); });
    return vbox({
               text("Quaternion Slerp interpolation") | bold | center,
               separator(),
               scene | border,
               text(QuaternionText("q0", q0)),
               text(QuaternionText("q1", q1)),
               text(QuaternionText("q(t)", current)),
               hbox({text("t = " + Format(t) + "   "), gauge(t) | flex}) |
                   border,
               text(std::string(paused ? "[PAUSED]" : "[PLAYING]") +
                    "   Space: pause/resume   |   q: quit") |
                   (paused ? color(Color::Yellow) : dim) | center,
               text("Red/Green/Cyan: current local axes   Yellow: interpolated Z tip") |
                   dim | center,
           }) |
           border;
  });

  auto component = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    if (event == Event::Character(' ')) {
      paused = !paused;
      return true;
    }
    return false;
  });

  screen.Loop(component);
  return 0;
}

