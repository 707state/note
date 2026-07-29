import { defineApp, quit } from "zigzag";

// JavaScript describes application state and behavior. It never controls raw
// terminal I/O or runs its own polling loop; Zig drives every callback.
let count = 0;

defineApp({
  init() {
    count = 0;
  },

  onKey(event) {
    if (event.key === "escape" || (event.ctrl && event.text === "c") || event.text === "q") {
      quit();
      return;
    }
    if (event.key === "up" || event.text === "+" || event.text === "=") count += 1;
    if (event.key === "down" || event.text === "-" || event.text === "_") count -= 1;
    if (event.text === "0") count = 0;
  },

  view({ width, height }) {
    const title = "QuickJS application, hosted by Zig";
    const body = `Count: ${count}`;
    const help = "↑/↓ or +/- count · 0 reset · q / Esc quit";
    const rule = "─".repeat(Math.max(0, Math.min(width - 2, 60)));
    const blankLines = Math.max(0, height - 7);
    return [
      `┌${rule}┐`,
      `│ ${title}`,
      `├${rule}┤`,
      `│ ${body}`,
      `│ ${help}`,
      `└${rule}┘`,
      ...Array(blankLines).fill(""),
    ].join("\n");
  },
});

