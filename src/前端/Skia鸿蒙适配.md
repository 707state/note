<!--toc:start-->
- [工作记录](#工作记录)
- [准备工作](#准备工作)
- [开始！](#开始)
  - [方法](#方法)
    - [坑！](#坑)
- [后续](#后续)
<!--toc:end-->

# 工作记录

干活的时候遇到了Skia没有鸿蒙适配（获取不了字体）的情况，为了能够在鸿蒙上渲染出来字体不得不看看Flutter怎么适配的鸿蒙！！！

# 准备工作

编译环境啥的概括一下，就是OHOS NDK、lycium工具以及Skia仓库。

Skia直接从Google的仓库拉就可以，gclient sync、git-sync-deps执行一下。

重点是Flutter鸿蒙！

_找到[这个仓库](https://gitee.com/openharmony-sig/flutter_engine)_。

这个仓库下面的attachment/repos/skia.patch就是重点了。

# 开始！

鸿蒙的Flutter使用的是一个比较旧的Skia（看看commit hash就知道了，而我干活的时候用的是更旧的版本。。。），如果想要直接git apply建议是checkout到旧的提交上再apply。

但是，如果想要用新的版本呢？这里我给出一个可行方法。

## 方法

我遇到的问题是，Skia版本差距有点大，patch用不了，所以看了下patch内容，主要都是font相关的，那其实就很简单了，只要把patch里面的代码和构建工具放进去不就完事？

新建src/ports/skia\_ohos文件夹，把以下文件挨个复制进去：

- SkDebug_ohos_cpp
- FontConfig_ohos.cpp/h
- FontInfo_ohos.h
- SkFontMgr_ohos.cpp/h
- SkFontMgr_ohos_factory.cpp
- SkFontStyleSet_ohos.cpp/h
- SkTypeface_ohos.cpp/h

还有把fontconfig.json放到src/ports/skia\_ohos/config里面，这个文件和/system/etc/fontconfig.json是一致的。

然后去添加BUILD.gn，注意在fontmgr\_ohos这一项里面，他的//thid\_party/skia/src/xxx这里面会有点小问题，直接替换成src/开头的路径就可以。

### 坑！

注意，鸿蒙的字体部分需要json格式的解析，这就是为什么你会在BUILD.gn里面看到sources/include\_dirs里面看到jsoncpp。

拉取[仓库](https://github.com/open-source-parsers/jsoncpp) 到thid\_party里面，进入到jsoncpp目录，修改amalgamate.py里面的header.add\_text部分加一个:

```python
header.add_text("#define JSON_USE_EXCEPTION 0")
```

执行amalgamate.py，得到一个dist目录，然后回到BUILD.gn，fontmgr\_ohos部分的sources jsoncpp部分改为"//third\_party/jsoncpp/dist/jsoncpp.cpp"，在include\_dirs写"//third\_party/jsoncpp/dist"。

这是主要的patch内容的迁移，如果有问题，请注意是否启用了freetype、patch的其他部分（SkTypes.h是否迁移了）有没有改对。

还有一个问题是，我遇到了一个版本差异问题，就是有一个类的构造函数参数对不上，这些都比较好改了。

# 后续

有机会在最新版本的skia看看能不能用这个patch，比较Flutter更新速度太恐怖了。
