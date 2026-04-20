const { execSync } = require("child_process");
const https = require("https");
const fs = require("fs");
const path = require("path");

const dir         = __dirname;
const fontFile    = path.join(dir, "open_huninn.ttf");
const symbolsFile = path.join(dir, "symbols.txt");
const SIZES       = [14, 18, 24, 32, 48];

function download(url, dest) {
    return new Promise((resolve, reject) => {
        const file = fs.createWriteStream(dest);
        https.get(url, { headers: { "User-Agent": "lv_font_gen" } }, res => {
            if (res.statusCode === 302 || res.statusCode === 301) {
                file.close();
                fs.unlinkSync(dest);
                return download(res.headers.location, dest).then(resolve).catch(reject);
            }
            res.pipe(file);
            file.on("finish", () => file.close(resolve));
        }).on("error", err => {
            fs.unlinkSync(dest);
            reject(err);
        });
    });
}

async function fetchFontUrl() {
    return new Promise((resolve, reject) => {
        https.get(
            "https://api.github.com/repos/justfont/open-huninn-font/releases/latest",
            { headers: { "User-Agent": "lv_font_gen" } },
            res => {
                let data = "";
                res.on("data", chunk => data += chunk);
                res.on("end", () => {
                    const release = JSON.parse(data);
                    const asset = release.assets.find(a => a.name.endsWith(".ttf"));
                    if (!asset) return reject(new Error("找不到 .ttf 資源"));
                    resolve(asset.browser_download_url);
                });
            }
        ).on("error", reject);
    });
}

async function main() {
    if (!fs.existsSync(fontFile)) {
        console.log("[LVGL Font] 字體不存在，從 GitHub 下載最新版粉圓字體 ...");
        const url = await fetchFontUrl();
        console.log(`[LVGL Font] 下載來源: ${url}`);
        await download(url, fontFile);
        console.log("[LVGL Font] 下載完成");
    }

    if (!fs.existsSync(symbolsFile)) {
        console.error(`[ERROR] Symbols not found: ${symbolsFile}`);
        process.exit(1);
    }

    const symbols = fs.readFileSync(symbolsFile, "utf8")
        .split("\n")
        .filter(l => !l.startsWith("#") && l.trim() !== "")
        .join("")
        .replace(/\s/g, "");

    for (const size of SIZES) {
        const outputFile = path.join(dir, `font_huninn_${size}.c`);
        console.log(`[LVGL Font] 轉換 size ${size} ...`);
        execSync(
            `lv_font_conv --bpp 4 --size ${size} --no-compress` +
            ` --font "${fontFile}" --symbols "${symbols}" --range 32-127` +
            ` --format lvgl --lv-include lvgl.h -o "${outputFile}"`,
            { stdio: "inherit" }
        );
        console.log(`[SUCCESS] ${outputFile}`);
    }

    process.exit(0);
}

main().catch(err => { console.error("[ERROR]", err.message); process.exit(1); });
