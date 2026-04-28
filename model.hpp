#pragma once
#include "Eigen/Core"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

// ============================================================
// 数据结构
// ============================================================

struct Sample {
  Eigen::VectorXf image; // [784]  归一化到 [0,1]
  Eigen::VectorXf label; // [10]   one-hot
};

// ============================================================
// MNIST 数据加载
// ============================================================

class MnistData {
public:
  MnistData(const std::string &images_path, const std::string &labels_path) {
    // 读取图片文件
    std::ifstream ifs(images_path, std::ios::binary);
    if (!ifs)
      throw std::runtime_error("无法打开图片文件: " + images_path);
    readBigEndianInt(ifs); // magic number
    num_images = readBigEndianInt(ifs);
    num_rows   = readBigEndianInt(ifs);
    num_cols   = readBigEndianInt(ifs);

    const uint32_t pixel_count = num_rows * num_cols;
    raw_images.resize(num_images, std::vector<float>(pixel_count));
    for (uint32_t i = 0; i < num_images; i++)
      for (uint32_t j = 0; j < pixel_count; j++)
        raw_images[i][j] = static_cast<uint8_t>(ifs.get()) / 255.0f;
    ifs.close();

    // 读取标签文件
    std::ifstream lfs(labels_path, std::ios::binary);
    if (!lfs)
      throw std::runtime_error("无法打开标签文件: " + labels_path);
    readBigEndianInt(lfs); // magic number
    uint32_t num_labels = readBigEndianInt(lfs);
    raw_labels.resize(num_labels);
    for (uint32_t i = 0; i < num_labels; i++)
      raw_labels[i] = static_cast<uint8_t>(lfs.get());
    lfs.close();

    std::cout << std::format("读取完成: {} 张图片\n", num_images);
  }

  // 返回 Sample 列表（image 为 VectorXf，label 为 one-hot VectorXf）
  std::vector<Sample> samples() const {
    std::vector<Sample> result(num_images);
    for (size_t i = 0; i < num_images; ++i) {
      result[i].image = Eigen::Map<const Eigen::VectorXf>(
          raw_images[i].data(), static_cast<Eigen::Index>(raw_images[i].size()));
      result[i].label = Eigen::VectorXf::Zero(10);
      result[i].label[raw_labels[i]] = 1.0f;
    }
    return result;
  }

  // 返回原始标签（推理时用，不需要 one-hot）
  std::vector<uint8_t> labels() const { return raw_labels; }

  uint32_t size() const { return num_images; }

private:
  static uint32_t readBigEndianInt(std::ifstream &ifs) {
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
      result <<= 8;
      result |= static_cast<uint8_t>(ifs.get());
    }
    return result;
  }

  uint32_t num_images{}, num_rows{}, num_cols{};
  std::vector<std::vector<float>> raw_images;
  std::vector<uint8_t>            raw_labels;
};

// ============================================================
// 全连接层
// ============================================================

class Linear {
public:
  Eigen::MatrixXf W, dW; // [out × in]
  Eigen::VectorXf b, db; // [out]
  Eigen::VectorXf input_cache;

  // 随机初始化（He 初始化，适合 ReLU）
  Linear(int in_features, int out_features) {
    float scale = std::sqrt(2.0f / static_cast<float>(in_features));
    W  = Eigen::MatrixXf::Random(out_features, in_features) * scale;
    b  = Eigen::VectorXf::Zero(out_features);
    dW = Eigen::MatrixXf::Zero(out_features, in_features);
    db = Eigen::VectorXf::Zero(out_features);
  }

  // 占位构造（用于 load()）
  Linear() : Linear(1, 1) {}

  Eigen::VectorXf forward(const Eigen::VectorXf &x) {
    input_cache = x;
    return W * x + b;
  }

  Eigen::VectorXf backward(const Eigen::VectorXf &delta) {
    dW = delta * input_cache.transpose();
    db = delta;
    return W.transpose() * delta;
  }

  void update(float lr) {
    W -= lr * dW;
    b -= lr * db;
  }

  // ---- 序列化 ----

  void save(std::ofstream &ofs) const {
    auto write_mat = [&](const Eigen::MatrixXf &m) {
      int r = static_cast<int>(m.rows()), c = static_cast<int>(m.cols());
      ofs.write(reinterpret_cast<const char *>(&r), sizeof(int));
      ofs.write(reinterpret_cast<const char *>(&c), sizeof(int));
      ofs.write(reinterpret_cast<const char *>(m.data()),
                static_cast<std::streamsize>(r * c * sizeof(float)));
    };
    auto write_vec = [&](const Eigen::VectorXf &v) {
      int sz = static_cast<int>(v.size());
      ofs.write(reinterpret_cast<const char *>(&sz), sizeof(int));
      ofs.write(reinterpret_cast<const char *>(v.data()),
                static_cast<std::streamsize>(sz * sizeof(float)));
    };
    write_mat(W);
    write_vec(b);
  }

  void load(std::ifstream &ifs) {
    auto read_mat = [&](Eigen::MatrixXf &m) {
      int r, c;
      ifs.read(reinterpret_cast<char *>(&r), sizeof(int));
      ifs.read(reinterpret_cast<char *>(&c), sizeof(int));
      m.resize(r, c);
      ifs.read(reinterpret_cast<char *>(m.data()),
               static_cast<std::streamsize>(r * c * sizeof(float)));
    };
    auto read_vec = [&](Eigen::VectorXf &v) {
      int sz;
      ifs.read(reinterpret_cast<char *>(&sz), sizeof(int));
      v.resize(sz);
      ifs.read(reinterpret_cast<char *>(v.data()),
               static_cast<std::streamsize>(sz * sizeof(float)));
    };
    read_mat(W);
    read_vec(b);
    dW = Eigen::MatrixXf::Zero(W.rows(), W.cols());
    db = Eigen::VectorXf::Zero(b.size());
  }
};

// ============================================================
// 激活函数 & 损失
// ============================================================

inline Eigen::VectorXf relu(const Eigen::VectorXf &x) {
  return x.cwiseMax(0.0f);
}

inline Eigen::VectorXf relu_backward(const Eigen::VectorXf &delta,
                                      const Eigen::VectorXf &x) {
  return delta.cwiseProduct((x.array() > 0.0f).cast<float>().matrix());
}

inline Eigen::VectorXf softmax(const Eigen::VectorXf &z) {
  Eigen::VectorXf e = (z.array() - z.maxCoeff()).exp();
  return e / e.sum();
}

inline float cross_entropy(const Eigen::VectorXf &p,
                            const Eigen::VectorXf &y) {
  return -(y.array() * p.array().log()).sum();
}

// Softmax + CrossEntropy 联合反向（δ = p - y）
inline Eigen::VectorXf softmax_ce_backward(const Eigen::VectorXf &p,
                                            const Eigen::VectorXf &y) {
  return p - y;
}
