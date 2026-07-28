import * as zz from "zigzag";

zz.init({ altScreen: true, hideCursor: true });

try {
    let count = 0;
    let running = true;

    while (running) {
        const [w, h] = zz.termSize();
        zz.clear();

        const title = zz.render(" QuickJS + ZigZag TUI Demo ", {
            fg: [255, 255, 255], bg: [80, 80, 180], bold: true, padding: 1, width: w, align: "center",
        });
        zz.writeAt(0, 0, title);

        const countText = "Count: " + String(count);
        const signColor = count > 0 ? [0, 200, 0] : count < 0 ? [200, 0, 0] : [200, 200, 200];
        const box = zz.render(countText, {
            fg: signColor, bold: true,
            border: "rounded", borderColor: [0, 200, 200],
            padding: [2, 4, 2, 4], width: 40, align: "center",
        });
        zz.writeAt(3, 0, box);

        const help = zz.render("q/ESC quit | arrows/+/- count | 0 reset", {
            fg: [160, 160, 160], dim: true, width: w, align: "center",
        });
        zz.writeAt(h - 1, 0, help);

        const ev = zz.pollEvent(50);
        if (ev) {
            if (ev.key === "escape") running = false;
            if (ev.key === "up") count++;
            if (ev.key === "down") count--;
            if (ev.char === "q") running = false;
            if (ev.char === "+" || ev.char === "=") count++;
            if (ev.char === "-" || ev.char === "_") count--;
            if (ev.char === "0") count = 0;
            if (ev.ctrl && ev.char === "c") running = false;
        }
    }
} finally {
    zz.deinit();
}
