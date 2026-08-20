
// Fake-3D demo: a rotating tetrahedron rendered with the FTXUI canvas.
// Pipeline: rotate vertices -> perspective project -> backface culling ->
// draw wireframe edges colored per visible face, in a terminal animation loop.
#include <chrono>                      // for steady_clock, duration
#include <cmath>                       // for fmod
#include <ftxui/component/app.hpp>     // for App, Loop via App::Loop
#include <ftxui/component/component.hpp>  // for Renderer, CatchEvent, Event
#include <ftxui/dom/elements.hpp>      // for canvas, border, Element
#include <ftxui/screen/color.hpp>      // for Color, Color::Red...
#include <vector>                      // for vector

#include <Eigen/Geometry>              // for Vector3f, AngleAxis, Matrix3f

#include "ftxui/dom/canvas.hpp"  // for Canvas

namespace {

constexpr float kPi = 3.14159265358979f;

using Vec3 = Eigen::Vector3f;
using Vec2 = Eigen::Vector2f;

// Build 3x3 rotation matrices from Eigen's axis/angle representation.
Eigen::Matrix3f RotationY(float a) {
  return Eigen::AngleAxis<float>(a, Eigen::Vector3f::UnitY()).toRotationMatrix();
}
Eigen::Matrix3f RotationX(float a) {
  return Eigen::AngleAxis<float>(a, Eigen::Vector3f::UnitX()).toRotationMatrix();
}

// Regular tetrahedron: unit-ish vertices, center at origin.
std::vector<Vec3> TetrahedronVertices() {
  return {
      Vec3(1.0f, 1.0f, 1.0f),
      Vec3(1.0f, -1.0f, -1.0f),
      Vec3(-1.0f, 1.0f, -1.0f),
      Vec3(-1.0f, -1.0f, 1.0f),
  };
}

// Faces are triples of vertex indices, wound counter-clockwise when viewed
// from outside, so the outward normal is Cross(b-a, c-a).
constexpr int kFaces[4][3] = {
    {0, 1, 2},
    {0, 3, 1},
    {0, 2, 3},
    {1, 3, 2},
};

const ftxui::Color kFaceColors[4] = {
    ftxui::Color::Red,
    ftxui::Color::Green,
    ftxui::Color::Blue,
    ftxui::Color::Yellow,
};

// Dimmer variants used for back-facing (occluded) faces: the "translucent"
// back layer shows through behind the bright front faces.
const ftxui::Color kFaceColorsDim[4] = {
    ftxui::Color::RGB(100, 30, 30),   // red, dimmed
    ftxui::Color::RGB(30, 100, 30),   // green, dimmed
    ftxui::Color::RGB(30, 30, 120),   // blue, dimmed
    ftxui::Color::RGB(110, 100, 25),  // yellow, dimmed
};

// One frame: project rotated vertices, cull back faces, draw edges.
void DrawFrame(ftxui::Canvas& c, float angle_y, float angle_x) {
  // Project a world point with weak perspective onto the canvas.
  const float eye_z = 6.0f;    // camera distance along +z
  const float focal = 90.0f;   // focal length in pixels
  const float cx = c.width() * 0.5f;
  const float cy = c.height() * 0.5f;
  auto project = [&](const Vec3& v) -> Vec2 {
    // Camera looks down -z, world +z goes away from camera. Rotate first.
    float depth = eye_z - v.z();  // distance from eye
    if (depth < 0.1f)
      depth = 0.1f;
    float s = focal / depth;
    return Vec2(cx + v.x() * s, cy - v.y() * s);
  };

  // Rotate (Y then X) and project all vertices. Rotation matrices are
  // multiplied once and applied to every vertex.
  const Eigen::Matrix3f rot = RotationY(angle_y) * RotationX(angle_x);
  std::vector<Vec3> rotated = TetrahedronVertices();
  for (auto& v : rotated) {
    v = rot * v;
  }
  Vec2 pts[4];
  for (int i = 0; i < 4; i++)
    pts[i] = project(rotated[i]);
  // Translucent rendering: instead of culling, draw back-facing faces first
  // in dim colors, then front-facing faces in bright colors on top. Where a
  // bright edge overlaps a dim one, the bright one wins (last writer wins in
  // Canvas::Style), so occluded edges faintly show through — a fake
  // translucency via painter's algorithm.
  for (int pass = 0; pass < 2; pass++) {
    bool want_visible = (pass == 1);
    for (int f = 0; f < 4; f++) {
      const auto& face = kFaces[f];
      Vec3 a = rotated[face[0]];
      Vec3 b = rotated[face[1]];
      Vec3 cc = rotated[face[2]];
      Vec3 normal = (b - a).cross(cc - a).normalized();
      Vec3 view_dir(0.0f, 0.0f, eye_z);  // from face point toward the eye
      bool visible = normal.dot(view_dir - a) > 0.0f;
      if (visible != want_visible)
        continue;
      const ftxui::Color& color =
          visible ? kFaceColors[f] : kFaceColorsDim[f];
      // Draw the triangle's three edges in this face's color.
      for (int e = 0; e < 3; e++) {
        const Vec2& p1 = pts[face[e]];
        const Vec2& p2 = pts[face[(e + 1) % 3]];
        c.DrawPointLine(int(p1.x()), int(p1.y()), int(p2.x()), int(p2.y()),
                        color);
      }
    }
  }
}

}  // namespace

int main() {
  using namespace ftxui;

  auto screen = App::FitComponent();

  // Rotation angle is derived from wall-clock time so the animation speed is
  // independent of how often a frame is actually drawn.
  const auto start = std::chrono::steady_clock::now();
  auto document = canvas(60, 60, [&](Canvas& c) {
    float elapsed =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - start)
            .count();
    float angle_y = std::fmod(elapsed * 0.5f, 2.0f * kPi);
    DrawFrame(c, angle_y, 0.6f);
  }) | border;

  auto renderer = Renderer([&] {
    // Request the next frame so the tetrahedron keeps spinning. FTXUI owns
    // screen clearing, cursor rewinding and terminal flushing; we never call
    // Print/ResetPosition/fflush ourselves.
    screen.RequestAnimationFrame();
    return document;
  });

  auto component = CatchEvent(renderer, [&](Event event) {
    if (event == Event::Character('q') || event == Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    return false;
  });

  screen.Loop(component);
  return 0;
}

