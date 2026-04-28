#include "model.hpp"
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <iostream>

int main() {
  // ---- 路径定位 ----
  auto base        = boost::dll::program_location().parent_path().parent_path();
  auto images_path = (base / "dataset" / "t10k-images-idx3-ubyte").string();
  auto labels_path = (base / "dataset" / "t10k-labels-idx1-ubyte").string();
  auto model_path  = (base / "dataset" / "mnist_weights.bin").string();

  if (!boost::filesystem::exists(model_path)) {
    std::cerr << "找不到模型文件: " << model_path
              << "\n请先运行 mnist 程序完成训练。\n";
    return 1;
  }
  if (!boost::filesystem::exists(images_path) ||
      !boost::filesystem::exists(labels_path)) {
    std::cerr << std::format("找不到测试数据:\n  {}\n  {}\n",
                             images_path, labels_path);
    return 1;
  }

  // ---- 加载模型权重 ----
  Linear layer1, layer2;
  {
    std::ifstream ifs(model_path, std::ios::binary);
    if (!ifs) {
      std::cerr << "无法读取模型文件: " << model_path << "\n";
      return 1;
    }
    layer1.load(ifs);
    layer2.load(ifs);
  }
  std::cout << std::format("模型加载成功: {}\n", model_path);

  // ---- 加载测试集 ----
  MnistData test_data(images_path, labels_path);
  auto raw_labels = test_data.labels();
  auto samples    = test_data.samples();
  const int n     = static_cast<int>(samples.size());

  // ---- 推理 ----
  int correct = 0;
  // 各数字的正确数 / 总数，用于 per-class 统计
  std::array<int, 10> class_correct{};
  std::array<int, 10> class_total{};

  for (int i = 0; i < n; ++i) {
    const Sample &s = samples[i];

    auto z1 = layer1.forward(s.image);
    auto a1 = relu(z1);
    auto z2 = layer2.forward(a1);
    auto p  = softmax(z2);

    Eigen::Index pred;
    p.maxCoeff(&pred);
    int truth = static_cast<int>(raw_labels[i]);

    if (static_cast<int>(pred) == truth) {
      ++correct;
      ++class_correct[truth];
    }
    ++class_total[truth];
  }

  // ---- 输出结果 ----
  float overall_acc = static_cast<float>(correct) / n * 100.0f;
  std::cout << std::format("\n测试集总体准确率: {}/{} = {:.2f}%\n\n",
                           correct, n, overall_acc);

  std::cout << "各数字准确率:\n";
  for (int d = 0; d < 10; ++d) {
    float acc = static_cast<float>(class_correct[d]) /
                static_cast<float>(class_total[d]) * 100.0f;
    std::cout << std::format("  数字 {}:  {:4d}/{:4d}  ({:.2f}%)\n",
                             d, class_correct[d], class_total[d], acc);
  }

  return 0;
}
