/*
 * Copy the ESP32 firmware web UI into this Capacitor app.
 * data remains the single source of truth for the web pages.
 */
import { readdir, readFile, writeFile, copyFile, mkdir, stat, rm } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { dirname, join, extname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const DATA_DIR = resolve(__dirname, '../../data');
const SRC_DIR = resolve(__dirname, 'src');
const PUBLIC_DIR = resolve(SRC_DIR, 'public');

// Wi-Fi provisioning, the firmware menu, and the setup/display pages are not part of
// the native app. The app has its own menu and controller settings page.
const SKIP = new Set(['wifimanager.html', 'wifi.json', 'menu.js', 'settings.html', 'setup.html', 'display.html']);
const ASSET_EXT = new Set(['.js', '.css', '.png', '.ico', '.jpg', '.svg', '.gif']);

function injectAppScripts(html) {
  if (!html.includes('device-config.js')) {
    const shim = '    <script src="device-config.js"></script>\n';
    html = html.includes('<head>')
      ? html.replace('<head>', '<head>\n' + shim)
      : shim + html;
  }
  if (!html.includes('app-menu.css')) {
    const appCss = '    <link rel="stylesheet" href="app-menu.css">\n';
    html = html.includes('</head>')
      ? html.replace('</head>', appCss + '</head>')
      : appCss + html;
  }

  const menuTagRe = /<script[^>]*src=["']\.?\/?menu\.js["'][^>]*><\/script>/i;
  if (menuTagRe.test(html)) {
    html = html.replace(menuTagRe, '<script src="app-menu.js"></script>');
  }
  return html;
}

async function main() {
  if (!existsSync(DATA_DIR)) {
    throw new Error(`firmware data directory not found: ${DATA_DIR}`);
  }

  await mkdir(PUBLIC_DIR, { recursive: true });
  for (const name of SKIP) {
    await rm(join(PUBLIC_DIR, name), { force: true });
  }
  const entries = await readdir(DATA_DIR);
  let htmlCount = 0;
  let assetCount = 0;

  for (const name of entries) {
    if (SKIP.has(name)) continue;
    const source = join(DATA_DIR, name);
    const info = await stat(source);
    if (!info.isFile()) continue;

    const ext = extname(name).toLowerCase();
    if (ext === '.html') {
      const html = injectAppScripts(await readFile(source, 'utf8'));
      await writeFile(join(SRC_DIR, name), html, 'utf8');
      htmlCount++;
    } else if (ASSET_EXT.has(ext)) {
      await copyFile(source, join(PUBLIC_DIR, name));
      assetCount++;
    }
  }

  await copyFile(
    join(__dirname, 'app-scripts/settings.html'),
    join(SRC_DIR, 'settings.html')
  );
  htmlCount++;

  for (const appAsset of [
    'device-config.js',
    'app-menu.js',
    'app-menu.css',
    'app-settings.js',
    'app-settings.css'
  ]) {
    const source = join(__dirname, 'app-scripts', appAsset);
    await copyFile(source, join(PUBLIC_DIR, appAsset));
  }

  console.log(`sync-data: ${htmlCount} HTML + ${assetCount} firmware asset(s) -> src/`);
}

main().catch((error) => {
  console.error('sync-data failed:', error);
  process.exit(1);
});
