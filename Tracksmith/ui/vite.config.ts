import { defineConfig } from 'vite';
// Native WebView embeds index.html, app.js, and app.css. Inline launch artwork
// into those assets so it does not depend on an unserved external image file.
export default defineConfig({ base: './', resolve: { dedupe: ['react', 'react-dom'] }, build: { assetsInlineLimit: 3 * 1024 * 1024, assetsDir: '', rollupOptions: { output: { entryFileNames: 'app.js', assetFileNames: 'app.[ext]' } } } });

