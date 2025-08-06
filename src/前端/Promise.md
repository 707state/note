```javascript
class MyPromise {
  constructor(executor) {
    // Promise 的状态
    this.state = 'pending';
    // 保存成功的返回值
    this.value = undefined;
    // 保存失败的原因
    this.reason = undefined;
    // 存储后续的成功回调
    this.onFulfilledCallbacks = [];
    // 存储后续的失败回调
    this.onRejectedCallbacks = [];

    const resolve = (value) => {
      if (this.state === 'pending') {
        this.state = 'fulfilled';
        this.value = value;
        this.onFulfilledCallbacks.forEach(fn => fn());
      }
    };

    const reject = (reason) => {
      if (this.state === 'pending') {
        this.state = 'rejected';
        this.reason = reason;
        this.onRejectedCallbacks.forEach(fn => fn());
      }
    };

    // 执行 executor，并传入 resolve 和 reject
    try {
      executor(resolve, reject);
    } catch (error) {
      reject(error);
    }
  }

  then(onFulfilled, onRejected) {
    // onFulfilled 和 onRejected 为可选参数
    onFulfilled = typeof onFulfilled === 'function' ? onFulfilled : value => value;
    onRejected = typeof onRejected === 'function' ? onRejected : reason => { throw reason; };

    // 返回一个新的 Promise 对象
    const promise2 = new MyPromise((resolve, reject) => {
      if (this.state === 'fulfilled') {
        setTimeout(() => {
          try {
            const x = onFulfilled(this.value);
            resolvePromise(promise2, x, resolve, reject);
          } catch (error) {
            reject(error);
          }
        });
      } else if (this.state === 'rejected') {
        setTimeout(() => {
          try {
            const x = onRejected(this.reason);
            resolvePromise(promise2, x, resolve, reject);
          } catch (error) {
            reject(error);
          }
        });
      } else if (this.state === 'pending') {
        this.onFulfilledCallbacks.push(() => {
          setTimeout(() => {
            try {
              const x = onFulfilled(this.value);
              resolvePromise(promise2, x, resolve, reject);
            } catch (error) {
              reject(error);
            }
          });
        });

        this.onRejectedCallbacks.push(() => {
          setTimeout(() => {
            try {
              const x = onRejected(this.reason);
              resolvePromise(promise2, x, resolve, reject);
            } catch (error) {
              reject(error);
            }
          });
        });
      }
    });

    return promise2;
  }
}

function resolvePromise(promise2, x, resolve, reject) {
  if (promise2 === x) {
    return reject(new TypeError('Chaining cycle detected for promise'));
  }

  let called;
  if (x != null && (typeof x === 'object' || typeof x === 'function')) {
    try {
      const then = x.then;
      if (typeof then === 'function') {
        then.call(x, y => {
          if (called) return;
          called = true;
          resolvePromise(promise2, y, resolve, reject);
        }, r => {
          if (called) return;
          called = true;
          reject(r);
        });
      } else {
        resolve(x);
      }
    } catch (error) {
      if (called) return;
      called = true;
      reject(error);
    }
  } else {
    resolve(x);
  }
}
```


测试用例:

```javascript
function testPromise() {
  // 创建一个新的 MyPromise 实例
  const promise = new MyPromise((resolve, reject) => {
    // 模拟异步操作
    setTimeout(() => {
      resolve("Success!");
    }, 1000);
  });

  // 使用 then 方法来处理异步结果
  promise.then(result => {
    console.log(result); // 输出: "Success!"
    return "Next Success!";
  }).then(nextResult => {
    console.log(nextResult); // 输出: "Next Success!"
  }).catch(error => {
    console.error("Caught error:", error);
  });

  // 测试一下拒绝的情况
  const rejectPromise = new MyPromise((resolve, reject) => {
    // 模拟异步错误
    setTimeout(() => {
      reject("Failed!");
    }, 500);
  });

  rejectPromise.then(result => {
    console.log(result);
  }).catch(error => {
    console.error("Caught error:", error); // 输出: "Caught error: Failed!"
  });
}

testPromise();
```
