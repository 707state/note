## C++ 手写数字识别学习指南

### 1. 数据集下载地址

* MNIST 手写数字数据集：

  * 官方网站: [http://yann.lecun.com/exdb/mnist/](http://yann.lecun.com/exdb/mnist/)
  * 直接下载链接：

    * 训练图像：`train-images-idx3-ubyte.gz`
    * 训练标签：`train-labels-idx1-ubyte.gz`
    * 测试图像：`t10k-images-idx3-ubyte.gz`
    * 测试标签：`t10k-labels-idx1-ubyte.gz`

### 2. 项目需要了解的知识

* **C++ 基础知识**：

  * 类与对象
  * 向量与矩阵操作（`std::vector`）
  * 文件读取与写入
  * 内存管理和指针基础
* **线性代数**：

  * 矩阵乘法
  * 向量点积
  * 矩阵转置
* **概率与统计**：

  * 概率分布基础
  * 交叉熵（Cross Entropy）
  * 均方误差（MSE）
* **机器学习基础**：

  * 神经网络结构（输入层、隐藏层、输出层）
  * 激活函数（Sigmoid, ReLU, Softmax）
  * 前向传播和反向传播算法
  * 梯度下降和学习率
* **数据处理**：

  * 数据归一化（0~1）
  * 批量训练（Batch Training）
  * one-hot 编码标签

### 3. 推荐书籍补足相关数学知识

* 《线性代数及其应用》 - David C. Lay
* 《概率论与数理统计》 - 茆诗松
* 《深度学习》 - Ian Goodfellow, Yoshua Bengio, Aaron Courville
* 《机器学习实战》 - Peter Harrington

### 4. 现有可供学习的项目

* **C++ 实现 MNIST 神经网络**

  * GitHub: [https://github.com/mnielsen/neural-networks-and-deep-learning](https://github.com/mnielsen/neural-networks-and-deep-learning) （Python版本，但可以参考算法）
  * GitHub C++ 示例: [https://github.com/Backpropagation/CppMNIST](https://github.com/Backpropagation/CppMNIST)
* **Tiny-dnn**（C++ 轻量神经网络库）：

  * [https://github.com/tiny-dnn/tiny-dnn](https://github.com/tiny-dnn/tiny-dnn)
* **其他 C++ 神经网络学习项目**：

  * [https://github.com/oreilly-japan/deep-learning-from-scratch](https://github.com/oreilly-japan/deep-learning-from-scratch)（Deep Learning from Scratch）

