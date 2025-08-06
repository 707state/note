# 什么是 Tree Shaking

Tree Shaking 是一种死代码消除（Dead Code Elimination）技术，通过静态分析代码依赖关系，移除未使用的代码，从而减小最终打包文件的体积。

# Tree Shaking 的核心原理

## ES6 模块的静态特性

Tree Shaking 依赖于 ES6 模块系统的静态特性：

```js
// ✅ ES6 模块 - 支持 Tree Shaking
// 导入导出在编译时确定
import { specificFunction } from './utils.js';
export const myFunction = () => {};

// ❌ CommonJS - 不支持 Tree Shaking
// 导入导出在运行时确定
const utils = require('./utils.js');
const { specificFunction } = utils; // 动态解构
module.exports = { myFunction: () => {} };
```
