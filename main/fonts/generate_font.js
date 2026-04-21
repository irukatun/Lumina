const { execSync } = require("child_process");
const https = require("https");
const fs = require("fs");
const path = require("path");

const dir = __dirname;

// ======================================================================
// 粉圓字體（中文 + ASCII）
// ======================================================================
const huninnTtf     = path.join(dir, "open_huninn.ttf");
const huninnSymbols = path.join(dir, "symbols.txt");
const HUNINN_SIZES  = [14, 18, 24, 32, 48];

// ======================================================================
// Material Symbols (Outlined) icon 字體
// ======================================================================
const iconTtf         = path.join(dir, "material_symbols.ttf");
const iconCodepoints  = path.join(dir, "material_symbols.codepoints");
const iconSymbols     = path.join(dir, "icons.txt");
const ICON_SIZES      = [14];
const ICON_TTF_URL        = "https://raw.githubusercontent.com/google/material-design-icons/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf";
const ICON_CODEPOINTS_URL = "https://raw.githubusercontent.com/google/material-design-icons/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.codepoints";

// ======================================================================
// 共用函式
// ======================================================================
function download(url, dest) {
    return new Promise((resolve, reject) => {
        const file = fs.createWriteStream(dest);
        https.get(url, { headers: { "User-Agent": "lv_font_gen" } }, res => {
            if (res.statusCode === 302 || res.statusCode === 301) {
                file.close();
                fs.unlinkSync(dest);
                return download(res.headers.location, dest).then(resolve).catch(reject);
            }
            if (res.statusCode !== 200) {
                file.close();
                fs.unlinkSync(dest);
                res.resume();
                return reject(new Error(`下載失敗 ${res.statusCode}: ${url}`));
            }
            res.pipe(file);
            file.on("finish", () => file.close(resolve));
        }).on("error", err => {
            if (fs.existsSync(dest)) fs.unlinkSync(dest);
            reject(err);
        });
    });
}

async function fetchHuninnUrl() {
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

// ======================================================================
// 粉圓字體
// ======================================================================
async function ensureHuninnTtf() {
    if (fs.existsSync(huninnTtf)) return;
    console.log("[LVGL Font] 粉圓字體不存在，從 GitHub 下載最新版 ...");
    const url = await fetchHuninnUrl();
    console.log(`[LVGL Font] 下載來源: ${url}`);
    await download(url, huninnTtf);
    console.log("[LVGL Font] 下載完成");
}

function generateHuninn() {
    if (!fs.existsSync(huninnSymbols)) {
        console.error(`[ERROR] Symbols 檔案不存在: ${huninnSymbols}`);
        process.exit(1);
    }

    const symbols = fs.readFileSync(huninnSymbols, "utf8")
        .split("\n")
        .filter(l => !l.startsWith("#") && l.trim() !== "")
        .join("")
        .replace(/\s/g, "");

    for (const size of HUNINN_SIZES) {
        const outputFile = path.join(dir, `font_huninn_${size}.c`);
        console.log(`[LVGL Font] 轉換粉圓 size ${size} ...`);
        execSync(
            `lv_font_conv --bpp 4 --size ${size} --no-compress` +
            ` --font "${huninnTtf}" --symbols "${symbols}" --range 32-127` +
            ` --format lvgl --lv-include lvgl.h -o "${outputFile}"`,
            { stdio: "inherit" }
        );
        console.log(`[SUCCESS] ${outputFile}`);
    }
}

// ======================================================================
// Material Symbols icon 字體
// ======================================================================
async function ensureIconAssets() {
    if (!fs.existsSync(iconTtf)) {
        console.log("[LVGL Font] Material Symbols 字體不存在，從 GitHub 下載 ...");
        console.log(`[LVGL Font] 下載來源: ${ICON_TTF_URL}`);
        await download(ICON_TTF_URL, iconTtf);
        console.log("[LVGL Font] 字體下載完成");
    }
    if (!fs.existsSync(iconCodepoints)) {
        console.log("[LVGL Font] Material Symbols codepoints 清單不存在，從 GitHub 下載 ...");
        console.log(`[LVGL Font] 下載來源: ${ICON_CODEPOINTS_URL}`);
        await download(ICON_CODEPOINTS_URL, iconCodepoints);
        console.log("[LVGL Font] codepoints 下載完成");
    }
}

// 讀取官方 codepoints 清單（每行 "icon_name hex_codepoint"），回傳 Map<name, hexUpper>
function loadIconCodepointMap() {
    const map = new Map();
    fs.readFileSync(iconCodepoints, "utf8")
        .split("\n")
        .forEach(line => {
            const parts = line.trim().split(/\s+/);
            if (parts.length === 2) map.set(parts[0], parts[1].toUpperCase());
        });
    return map;
}

function generateIconFont() {
    if (!fs.existsSync(iconSymbols)) {
        console.error(`[ERROR] icons 檔案不存在: ${iconSymbols}`);
        process.exit(1);
    }

    const nameToCp = loadIconCodepointMap();
    const wanted = fs.readFileSync(iconSymbols, "utf8")
        .split("\n")
        .map(l => l.trim())
        .filter(l => l && !l.startsWith("#"));

    const resolved = [];
    const missing  = [];
    for (const name of wanted) {
        const hex = nameToCp.get(name);
        if (hex) resolved.push({ name, hex });
        else missing.push(name);
    }

    if (missing.length > 0) {
        console.error(`[ERROR] icons.txt 內以下名稱在 Material Symbols 找不到:`);
        missing.forEach(n => console.error(`        - ${n}`));
        console.error(`        請到 https://fonts.google.com/icons 確認正確名稱`);
        process.exit(1);
    }
    if (resolved.length === 0) {
        console.error(`[ERROR] icons.txt 內無任何有效 icon 名稱`);
        process.exit(1);
    }

    const ranges = resolved.map(r => `0x${r.hex}`).join(",");
    console.log("[LVGL Font] 解析到的 icons:");
    resolved.forEach(r => console.log(`           ${r.name.padEnd(28)} 0x${r.hex}`));

    for (const size of ICON_SIZES) {
        const outputFile = path.join(dir, `font_mi_${size}.c`);
        console.log(`[LVGL Font] 轉換 Material Symbols size ${size} (${resolved.length} 個 icon) ...`);
        execSync(
            `lv_font_conv --bpp 4 --size ${size} --no-compress` +
            ` --font "${iconTtf}" --range ${ranges}` +
            ` --format lvgl --lv-include lvgl.h -o "${outputFile}"`,
            { stdio: "inherit" }
        );
        console.log(`[SUCCESS] ${outputFile}`);
    }
}

// ======================================================================
// 主流程
// ======================================================================
async function main() {
    await ensureHuninnTtf();
    await ensureIconAssets();
    generateHuninn();
    generateIconFont();
    process.exit(0);
}

main().catch(err => { console.error("[ERROR]", err.message); process.exit(1); });
