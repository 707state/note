
// Fake-3D demo: a rotating tetrahedron rendered with the FTXUI canvas.
// Pipeline: rotate vertices -> perspective project -> backface culling ->
// draw wireframe edges colored per visible face, in a terminal animation loop.
#include <algorithm>                // for max, sort
#include <cmath>                    // for cos, sin, fabs
#include <cstdio>                   // for fflush, printf
#include <ftxui/dom/elements.hpp>   // for canvas, border, Element, Fit
#include <ftxui/screen/color.hpp>   // for Color, Color::Red...
#include <ftxui/screen/screen.hpp>  // for Screen, Dimension
#include <chrono>                   // for milliseconds
#include <thread>                   // for sleep_for
#include <vector>                   // for vector

#include "ftxui/dom/canvas.hpp"  // for Canvas
#include "ftxui/dom/node.hpp"    // for Render
namespace {

constexpr float kPi = 3.14159265358979f;

struct Vec3 {
  float x, y, z;
};

struct Vec2 {
  float x, y;
};

Vec3 RotateY(Vec3 v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

Vec3 RotateX(Vec3 v, float a) {
  float c = std::cos(a), s = std::sin(a);
  return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 Cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 Normalize(Vec3 v) {
  float len = std::sqrt(Dot(v, v));
  return len > 1e-6f ? Vec3{v.x / len, v.y / len, v.z / len} : v;
}
Vec3 operator-(Vec3 a, Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Regular tetrahedron: unit-ish vertices, center at origin.
std::vector<Vec3> TetrahedronVertices() {
  return {
      {1.0f, 1.0f, 1.0f},
      {1.0f, -1.0f, -1.0f},
      {-1.0f, 1.0f, -1.0f},
      {-1.0f, -1.0f, 1.0f},
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
  auto project = [&](Vec3 v) -> Vec2 {
    // Camera looks down -z, world +z goes away from camera. Rotate first.
    float depth = eye_z - v.z;  // distance from eye
    if (depth < 0.1f)
      depth = 0.1f;
    float s = focal / depth;
    return {cx + v.x * s, cy - v.y * s};
  };

  // Rotate and project all vertices.
  std::vector<Vec3> rotated = TetrahedronVertices();
  for (auto& v : rotated) {
    v = RotateX(RotateY(v, angle_y), angle_x);
    // Keep world y up on screen: canvas y grows downward, so project negates.
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
      Vec3 normal = Normalize(Cross(b - a, cc - a));
      Vec3 view_dir = {0.0f, 0.0f, eye_z};  // from face point toward the eye
      Vec3 face_point = a;
      Vec3 to_eye = {view_dir.x - face_point.x, view_dir.y - face_point.y,
                     view_dir.z - face_point.z};
      bool visible = Dot(normal, to_eye) > 0.0f;
      if (visible != want_visible)
        continue;
      const ftxui::Color& color =
          visible ? kFaceColors[f] : kFaceColorsDim[f];
      // Draw the triangle's three edges in this face's color.
      for (int e = 0; e < 3; e++) {
        const Vec2& p1 = pts[face[e]];
        const Vec2& p2 = pts[face[(e + 1) % 3]];
        c.DrawPointLine(int(p1.x), int(p1.y), int(p2.x), int(p2.y), color);
      }
    }
  }
}

}  // namespace

int main() {
  using namespace ftxui;

  auto document = canvas(60, 60, [](Canvas& c) {
    static float t = 0.0f;
    t = std::fmod(t + 0.03f, 2.0f * kPi);
    DrawFrame(c, t, 0.6f);
  }) | border;

  auto screen = Screen::Create(Dimension::Fit(document));

  // Animate in place: render, print, then rewind the cursor so the next
  // frame overwrites the previous one.
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    screen.Clear();
    Render(screen, document);
    screen.Print();
    std::printf("%s", screen.ResetPosition().c_str());
    std::fflush(stdout);
  }

  return 0;
}
