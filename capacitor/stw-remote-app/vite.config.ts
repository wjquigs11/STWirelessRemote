import { defineConfig } from 'vite';
import { resolve } from 'node:path';
// HTML and assets are generated from ../../data by sync-data.mjs.
export default defineConfig({
  root: './src',
  build: {
    outDir: '../dist',
    minify: false,
    emptyOutDir: true,
    rollupOptions: {
      input: {
        main: resolve(__dirname, 'src/index.html'),
        settings: resolve(__dirname, 'src/settings.html'),
      },
    },
  },
});
