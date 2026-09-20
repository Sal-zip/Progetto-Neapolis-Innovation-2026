import { defineConfig } from 'vite';
import tailwindcss from '@tailwindcss/vite';

const BACKEND = 'http://localhost:5000';

export default defineConfig({
  plugins: [tailwindcss()],
  server: {
    proxy: {
      '/api': BACKEND,
      '/socket.io': { target: BACKEND, ws: true },
    },
  },
  // npm run build produce sempre la versione reale (.env.production), pronta per Flask,
  // dentro frontend/dist. La cartella si rigenera a ogni build e non finisce su GitHub.
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
});
