project(
  'mnist',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

eigen_dep = dependency('eigen3', required : true)
boost_dep = dependency('boost', modules : ['filesystem'])
deps = [eigen_dep, boost_dep]

# 训练程序：读取训练集，训练后保存 mnist_weights.bin
exe_train = executable(
  'mnist',
  'mnist.cpp',
  install : true,
  dependencies : deps,
)

# 推理程序：加载 mnist_weights.bin，跑测试集，输出准确率
exe_infer = executable(
  'infer',
  'infer.cpp',
  install : true,
  dependencies : deps,
)

# 图片推理程序：指定一张 JPG/PNG，输出预测数字和概率分布
exe_infer_image = executable(
  'infer_image',
  'infer_image.cpp',
  install : true,
  dependencies : deps,
)

test('basic', exe_train)
