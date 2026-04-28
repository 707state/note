#include "model.hpp"
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>

int main() {
  // ---- 路径定位（相对于可执行文件） ----
  auto base = boost::dll::program_location().parent_path().parent_path();
  auto images_path = (base / "dataset" / "train-images-idx3-ubyte").string();
  auto labels_path = (base / "dataset" / "train-labels-idx1-ubyte").string();
  auto model_path  = (base / "dataset" / "mnist_weights.bin").string();

  if (!boost::filesystem::exists(images_path) ||
      !boost::filesystem::exists(labels_path)) {
    std::cerr << std::format("找不到训练数据:\n  {}\n  {}\n",
                             images_path, labels_path);
    return 1;
  }

  // ---- 加载训练数据 ----
  MnistData data(images_path, labels_path);
  auto samples = data.samples();
  const int n = static_cast<int>(samples.size());

  // ---- 网络：784 → Linear(128) → ReLU → Linear(10) → Softmax ----
  Linear layer1(784, 128);
  Linear layer2(128, 10);

  // ---- 超参数 ----
  const float lr     = 0.01f;
  const int   epochs = 10;

  std::vector<int> indices(n);
  std::iota(indices.begin(), indices.end(), 0);
  std::mt19937 rng(42);

  // ---- 训练循环 ----
  for (int epoch = 0; epoch < epochs; ++epoch) {
    std::shuffle(indices.begin(), indices.end(), rng);

    float total_loss = 0.0f;
    int   correct    = 0;

    for (int idx : indices) {
      const Sample &s = samples[idx];

      // 前向
      auto z1 = layer1.forward(s.image);
      auto a1 = relu(z1);
      auto z2 = layer2.forward(a1);
      auto p  = softmax(z2);

      // 统计
      total_loss += cross_entropy(p, s.label);
      Eigen::Index pred, truth;
      p.maxCoeff(&pred);
      s.label.maxCoeff(&truth);
      if (pred == truth) ++correct;

      // 反向
      auto delta2 = softmax_ce_backward(p, s.label);
      auto da1    = layer2.backward(delta2);
      auto dz1    = relu_backward(da1, z1);
      layer1.backward(dz1);

      // 更新
      layer1.update(lr);
      layer2.update(lr);
    }

    std::cout << std::format("Epoch {:2d}  loss: {:.4f}  train_acc: {:.2f}%\n",
                             epoch + 1,
                             total_loss / n,
                             static_cast<float>(correct) / n * 100.0f);
  }

  // ---- 保存模型权重 ----
  std::ofstream ofs(model_path, std::ios::binary);
  if (!ofs) {
    std::cerr << "无法写入模型文件: " << model_path << "\n";
    return 1;
  }
  layer1.save(ofs);
  layer2.save(ofs);
  std::cout << std::format("模型已保存到: {}\n", model_path);

  return 0;
}
