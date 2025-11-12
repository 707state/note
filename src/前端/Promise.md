```javascript
function MyPromise(executor) {
  this.state = 'pending';
  this.value = undefined;
  this.handlers = []; // 存放成功 continuation
  this.catchers = []; // 存放失败 continuation

  const resolve = (val) => {
    if (this.state !== 'pending') return;
    // 如果 val 本身是 Promise，则等待其完成
    if (val instanceof MyPromise) {
      return val.then(resolve, reject);
    }
    this.state = 'fulfilled';
    this.value = val;
    this.handlers.forEach(h => h(val));
  };

  const reject = (err) => {
    if (this.state !== 'pending') return;
    this.state = 'rejected';
    this.value = err;
    this.catchers.forEach(c => c(err));
  };

  try {
    executor(resolve, reject);
  } catch (e) {
    reject(e);
  }
}

MyPromise.prototype.then = function (onFulfilled, onRejected) {
  return new MyPromise((resolve, reject) => {
    const handleFulfilled = (val) => {
      try {
        const result = typeof onFulfilled === 'function' ? onFulfilled(val) : val;
        resolve(result);
      } catch (e) {
        reject(e);
      }
    };

    const handleRejected = (err) => {
      try {
        if (typeof onRejected === 'function') {
          const result = onRejected(err);
          resolve(result);
        } else {
          reject(err); // 没有捕获则继续向下传递
        }
      } catch (e) {
        reject(e);
      }
    };

    if (this.state === 'fulfilled') {
      queueMicrotask(() => handleFulfilled(this.value));
    } else if (this.state === 'rejected') {
      queueMicrotask(() => handleRejected(this.value));
    } else {
      this.handlers.push(handleFulfilled);
      this.catchers.push(handleRejected);
    }
  });
};

MyPromise.prototype.catch = function (onRejected) {
  return this.then(null, onRejected);
};

MyPromise.prototype.finally = function (onFinally) {
  return this.then(
    (val) => {
      const res = onFinally && onFinally();
      if (res instanceof MyPromise) {
        return res.then(() => val);
      }
      return val;
    },
    (err) => {
      const res = onFinally && onFinally();
      if (res instanceof MyPromise) {
        return res.then(() => { throw err; });
      }
      throw err;
    }
  );
};


```


测试用例:

```javascript
new MyPromise((resolve) => {
  setTimeout(() => resolve(42), 1000);
})
.then(x => x + 1)
.then(y => { throw new Error("oops"); })
.catch(e => {
  console.log("Caught:", e.message);
  return 99;
})
.finally(() => console.log("Cleanup"))
.then(v => console.log("Final value:", v));
```
