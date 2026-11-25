import { access, copyFile, mkdir } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { homedir } from 'node:os';
import { join, resolve } from 'node:path';

const appDir = resolve(import.meta.dirname);
const apk = join(appDir, 'android', 'app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk');
const traditionalDropbox = join(homedir(), 'Dropbox');
const cloudStorageDropbox = join(homedir(), 'Library', 'CloudStorage', 'Dropbox');
const dropboxDir = existsSync(traditionalDropbox) ? traditionalDropbox : cloudStorageDropbox;
const destination = join(dropboxDir, 'stw-remote-debug.apk');

try {
  await access(apk);
  await mkdir(dropboxDir, { recursive: true });
  await copyFile(apk, destination);
  console.log(`Copied ${apk} -> ${destination}`);
} catch (error) {
  console.error(`Failed to copy APK to ${destination}:`, error);
  process.exit(1);
}
