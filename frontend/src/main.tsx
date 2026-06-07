import {StrictMode} from 'react';
import {createRoot} from 'react-dom/client';
import { LangProvider } from './i18n';
import App from './App.tsx';
import './index.css';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <LangProvider>
      <App />
    </LangProvider>
  </StrictMode>,
);
