// 从命令行指定的 JPG / PNG 文件推理单个手写数字
// 用法：./infer_image <图片路径>  [模型路径（可选）]
//
// 图片预处理流程：
//   1. 读取任意尺寸图片，转单通道灰度
//   2. 自动裁剪：找到前景像素的边界框，加 10% 留白后裁剪
//   3. 缩放到 28×28
//   4. 归一化到 [0,1]
//   5. 反色（白底黑字 → 黑底白字，与 MNIST 一致）
//   6. 将预处理结果保存为 debug_input.pgm，可用预览打开确认

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

#include "model.hpp"
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

// 将 28×28 数组保存为 PGM（可用 macOS 预览直接打开）
static void save_pgm(const std::string &path,
                     const std::array<uint8_t, 28 * 28> &pixels) {
  std::ofstream ofs(path, std::ios::binary);
  ofs << "P5\n28 28\n255\n";
  ofs.write(reinterpret_cast<const char *>(pixels.data()), 28 * 28);
}

struct BBox { int x0, y0, x1, y1; };

// 在灰度图中找到"前景"像素（与背景色差异大）的边界框
// threshold: 像素与背景均值的差超过此值则视为前景
static BBox find_foreground_bbox(const uint8_t *gray, int w, int h,
                                  int threshold = 30) {
  // 估计背景色：取四个角 5×5 均值
  long bg_sum = 0;
  int  bg_n   = 0;
  auto sample_corner = [&](int sx, int sy) {
    for (int dy = 0; dy < 5 && sy + dy < h; ++dy)
      for (int dx = 0; dx < 5 && sx + dx < w; ++dx) {
        bg_sum += gray[(sy + dy) * w + (sx + dx)];
        ++bg_n;
      }
  };
  sample_corner(0, 0);
  sample_corner(w - 5, 0);
  sample_corner(0, h - 5);
  sample_corner(w - 5, h - 5);
  int bg = static_cast<int>(bg_sum / bg_n);

  int x0 = w, y0 = h, x1 = 0, y1 = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (std::abs(static_cast<int>(gray[y * w + x]) - bg) > threshold) {
        x0 = std::min(x0, x); y0 = std::min(y0, y);
        x1 = std::max(x1, x); y1 = std::max(y1, y);
      }

  // 没找到前景 → 返回全图
  if (x0 > x1 || y0 > y1) return {0, 0, w - 1, h - 1};

  // 加 10% 留白
  int pad_x = std::max(2, (x1 - x0 + 1) / 10);
  int pad_y = std::max(2, (y1 - y0 + 1) / 10);
  return {
    std::max(0, x0 - pad_x),
    std::max(0, y0 - pad_y),
    std::min(w - 1, x1 + pad_x),
    std::min(h - 1, y1 + pad_y)
  };
}

struct LoadResult {
  Eigen::VectorXf img;
  std::array<uint8_t, 28 * 28> debug_pixels; // 发给模型的原始像素（未反色）
};

static LoadResult load_image(const std::string &path, bool invert) {
  int w, h, channels;
  uint8_t *raw = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (!raw)
    throw std::runtime_error("无法读取图片: " + path +
                             "\n原因: " + stbi_failure_reason());

  std::cout << std::format("原始尺寸: {}×{}  通道数: {}\n", w, h, channels);

  // 自动裁剪到数字边界
  BBox bb = find_foreground_bbox(raw, w, h);
  int cw = bb.x1 - bb.x0 + 1;
  int ch = bb.y1 - bb.y0 + 1;
  std::cout << std::format("前景边界框: ({},{}) → ({},{})  裁剪尺寸: {}×{}\n",
                           bb.x0, bb.y0, bb.x1, bb.y1, cw, ch);

  // 将裁剪区域扩展为正方形（短边用背景色填充），防止缩放时宽高比失真
  // 估计背景色：取原图左上角像素
  uint8_t bg_color = raw[0];
  int sq = std::max(cw, ch);
  // 正方形在原图中的偏移（把短边居中）
  int off_x = (sq - cw) / 2;
  int off_y = (sq - ch) / 2;

  std::vector<uint8_t> cropped(sq * sq, bg_color);
  for (int y = 0; y < ch; ++y)
    for (int x = 0; x < cw; ++x)
      cropped[(off_y + y) * sq + (off_x + x)] =
          raw[(bb.y0 + y) * w + (bb.x0 + x)];
  stbi_image_free(raw);
  cw = sq; ch = sq;

  // 缩放到 28×28
  std::array<uint8_t, 28 * 28> resized{};
  stbir_resize_uint8_linear(cropped.data(), cw, ch, 0,
                             resized.data(), 28, 28, 0,
                             STBIR_1CHANNEL);

  // ---- 对比度拉伸：把像素分布拉满 [0, 255] ----
  // 让笔画更黑、背景更白，减少灰度模糊带来的影响
  {
    uint8_t lo = 255, hi = 0;
    for (uint8_t v : resized) { lo = std::min(lo, v); hi = std::max(hi, v); }
    if (hi > lo) {
      for (uint8_t &v : resized)
        v = static_cast<uint8_t>((static_cast<int>(v) - lo) * 255 / (hi - lo));
    }
  }

  // ---- 反色（白底黑字 → 黑底白字）后做形态学膨胀 ----
  // 先反色，让前景（笔画）变成高值，再膨胀加粗笔画
  std::array<uint8_t, 28 * 28> inverted{};
  for (int i = 0; i < 784; ++i)
    inverted[i] = 255 - resized[i];

  // 3×3 最大值膨胀：每个像素取自身及8邻域的最大值，使笔画变粗
  std::array<uint8_t, 28 * 28> dilated{};
  for (int y = 0; y < 28; ++y) {
    for (int x = 0; x < 28; ++x) {
      uint8_t mx = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          int ny = y + dy, nx = x + dx;
          if (ny >= 0 && ny < 28 && nx >= 0 && nx < 28)
            mx = std::max(mx, inverted[ny * 28 + nx]);
        }
      dilated[y * 28 + x] = mx;
    }
  }

  LoadResult res;
  res.debug_pixels = dilated; // debug 保存膨胀后的像素（即模型实际收到的）

  // 归一化（膨胀后已是黑底白字，直接归一化；invert=false 则再反色）
  res.img.resize(784);
  for (int i = 0; i < 784; ++i) {
    float v = dilated[i] / 255.0f;
    res.img[i] = invert ? v : (1.0f - v);
  }
  return res;
}

static Eigen::Index predict(Linear &l1, Linear &l2,
                             const Eigen::VectorXf &img,
                             Eigen::VectorXf &out_prob) {
  auto z1 = l1.forward(img);
  auto a1 = relu(z1);
  auto z2 = l2.forward(a1);
  out_prob = softmax(z2);
  Eigen::Index pred;
  out_prob.maxCoeff(&pred);
  return pred;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "用法: " << argv[0] << " <图片路径> [模型路径]\n";
    return 1;
  }

  const std::string image_path = argv[1];

  std::string model_path;
  if (argc >= 3) {
    model_path = argv[2];
  } else {
    auto base = boost::dll::program_location().parent_path().parent_path();
    model_path = (base / "dataset" / "mnist_weights.bin").string();
  }

  if (!boost::filesystem::exists(model_path)) {
    std::cerr << "找不到模型文件: " << model_path
              << "\n请先运行 mnist 完成训练。\n";
    return 1;
  }
  if (!boost::filesystem::exists(image_path)) {
    std::cerr << "找不到图片: " << image_path << "\n";
    return 1;
  }

  // ---- 加载模型 ----
  Linear layer1, layer2;
  {
    std::ifstream ifs(model_path, std::ios::binary);
    layer1.load(ifs);
    layer2.load(ifs);
  }

  // ---- 加载并预处理图片 ----
  // invert=true：输入是白底黑字（普通截图/拍照）→ 内部反色+膨胀 → 黑底白字（MNIST格式）
  LoadResult res;
  try {
    res = load_image(image_path, /*invert=*/true);
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }

  // ---- 保存调试 PGM（黑底白字，与模型输入完全一致） ----
  save_pgm("debug_input.pgm", res.debug_pixels);
  std::cout << "\n已保存预处理结果: debug_input.pgm\n"
            << "  用 macOS 预览打开 —— 应该看到黑底白字的数字\n\n";

  // ---- 推理 ----
  Eigen::VectorXf prob;
  Eigen::Index pred = predict(layer1, layer2, res.img, prob);

  // ---- 输出 ----
  std::cout << std::format("预测数字: {}  （置信度 {:.1f}%）\n\n",
                           pred, prob[pred] * 100.0f);

  std::cout << "各数字概率:\n";
  for (int d = 0; d < 10; ++d) {
    int bar = static_cast<int>(prob[d] * 40);
    std::cout << std::format("  {} : {:6.2f}%  {}\n",
                             d, prob[d] * 100.0f,
                             std::string(bar, '#'));
  }

  return 0;
}
