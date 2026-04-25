---
title: 玩一下gleam
author: jask
tags:
    - ProgrammingLanguages
    - FunctionalProgramming
    - JavaScript
    - Erlang
date: 2026-04-20
---

# beam

很少见到Beam VM平台上的新语言，而且还是静态类型的语言，这和Erlang/Elixir差别可太大了。那就来简单了解一下这门新语言吧。

## features

首先gleam是一个应用序的语言，这意味着传递参数时会先求值表达式，keep this in mind。

其次gleam没有if/else来做分支控制，它提供了case和guards两种方式来做flow control。

并且gleam没有exceptions、macros、type classes、early returns等等等等特性，简而言之就是大量其他语言的生产环境代码强依赖的特性都不存在。

## spawn it!
先不考虑说Erlang还是JavaScript后端，先来实现一个spawn的能力吧。

gleam的stdlib功能可以说少得可怜，只有类型转换、string等功能，想要调用操作系统的接口只能利用external去调用erlang或者JavaScript，这就有点令人两眼一黑了。那就先来看JavaScript！

```typescript
export function runCommand(cmd: string, args: string[]): { stdout: string, stderr: string, exitCode: number } {
  // Bun.spawnSync 返回的对象包含 stdout/stderr/exitCode
  const result = Bun.spawnSync({
    cmd: [cmd, ...args],
    stdout: "pipe", // 捕获标准输出
    stderr: "pipe", // 捕获标准错误
  });

  return {
    stdout: new TextDecoder().decode(result.stdout),
    stderr: new TextDecoder().decode(result.stderr),
    exitCode: result.exitCode,
  };
}
```

这是一个使用bun runtime的JS函数，使用的是sync而非async，这也就意味着这里不会返回一个Promise而是直接返回字符串。相应的gleam代码：

```gleam
@external(javascript, "../../../../dist/index.js", "runCommand")
fn run_command(cmd: String, args: List(String)) -> String
```

这里面的相对路径先不要管，index.js的内容就是bun build出来的。

```gleam
 echo run_command("ls", [])
 // 运行结果：
 //src/getting_started_with_gleam.gleam:7
//js({"stdout":"build\nbun.lock\nCLAUDE.md\ndist\ngleam.toml\nindex.ts\nmanifest.toml\nnode_modules\npackage.json\nREADME.md\nsrc\ntest\ntsconfig.json\n", "stderr": "", "exitCode": 0 })
```

这个结果还是比较惊喜的，因为我并没有实现Erlang版本`run_command`，但是gleam编译器似乎并不检查这个。

那接下来就改成async版本的，这里需要使用`gleam_javascript`包。

```typescript
type CommandResult = {
  stdout: string;
  stderr: string;
  exitCode: number;
};

async function readStream(
  stream: ReadableStream<Uint8Array> | null | undefined,
): Promise<string> {
  if (!stream) {
    return "";
  }

  return await new Response(stream).text();
}

export async function runCommandAsync(
  cmd: string,
  args: string[],
): Promise<CommandResult> {
  const child = Bun.spawn({
    cmd: [cmd, ...args],
    stdout: "pipe",
    stderr: "pipe",
  });

  const [stdout, stderr, exitCode] = await Promise.all([
    readStream(child.stdout),
    readStream(child.stderr),
    child.exited,
  ]);

  return { stdout, stderr, exitCode };
}
```

这里并不直接返回Promise（其实差不多），但是是一个async函数，对应的gleam代码：

```gleam
@external(javascript, "../../../../dist/index.js", "runCommandAsync")
fn run_command(cmd: String, args: List(String)) -> Promise(dynamic.Dynamic)

fn decode_command_result(data: dynamic.Dynamic) -> Result(
  CommandResult,
  List(decode.DecodeError),
) {
  let decoder = {
    use stdout <- decode.field("stdout", decode.string)
    use stderr <- decode.field("stderr", decode.string)
    use exit_code <- decode.field("exitCode", decode.int)
    decode.success(CommandResult(stdout:, stderr:, exit_code:))
  }

  decode.run(data, decoder)
}

pub fn main() -> Promise(Nil) {
  run_command("ls", [])
  |> promise.map(fn(data) {
    case decode_command_result(data) {
      Ok(CommandResult(stdout:, stderr:, exit_code:)) -> {
        io.println("JS async call succeeded")
        io.println("exitCode: " <> int.to_string(exit_code))
        io.println(stdout)
        case stderr {
          "" -> Nil
          _ -> io.println(stderr)
        }
      }
      Error(_) -> io.println("JS async call returned an unexpected shape")
    }
  })
  |> promise.rescue(fn(error) {
    io.println(
      "JS async call failed before completion, error type: "
      <> dynamic.classify(error),
    )
    Nil
  })
}
```

只能说还算是能用，当然这全部都是JavaScript魔法，因为promise.map、promise.rescue都是JS的ffi。

## concurrency

现在看看Gleam这个语言本身是否存在并发能力呢？前面已知gleam拥有use机制，这个机制就是continuation的语法糖，并不意味着Gleam本身拥有很强的并发特性，似乎Gleam的并发机制只能依靠erlang和JS本身的机制了。

### Actor

gleam在Erlang target上能够利用Actor模型，可以写出来如下代码：

```gleam
import gleam/erlang/process.{type Subject}
import gleam/int
import gleam/io
import gleam/otp/actor

pub type Message {
  Increment
  Get(reply_with: Subject(Int))
  Stop
}

fn handle_message(count: Int, message: Message) -> actor.Next(Int, Message) {
  case message {
    Increment -> actor.continue(count + 1)

    Get(reply_with) -> {
      actor.send(reply_with, count)
      actor.continue(count)
    }

    Stop -> actor.stop()
  }
}

pub fn main() {
  let assert Ok(started) =
    actor.new(0)
    |> actor.on_message(handle_message)
    |> actor.start

  let counter = started.data

  actor.send(counter, Increment)
  actor.send(counter, Increment)
  actor.send(counter, Increment)

  let count = actor.call(counter, waiting: 1000, sending: Get)
  io.println("current count = " <> int.to_string(count))

  actor.send(counter, Stop)
}
```
