// TASK-1768: the translation helpers for tests that cut single functions out of v2.js and run them
// in a sandbox. Since TASK-1764 those functions call t(key, params) / setT(key, fallback); the page
// without a language pack answers with the built-in English of I18N_EN, and so do these helpers.
// Same rules as v2.js: {{name}} placeholders, plural key_one / key_other picked by params.count,
// a missing key returns the key itself (t) or the fallback (setT).
//
// Usage: const { t, setT } = i18nEn(v2jsSource); new Function(..., 't', 'setT', body)(..., t, setT)
export function i18nEn(src) {
  const m = /var I18N_EN = (\{[\s\S]*?\r?\n  \});/.exec(src);
  const EN = m ? JSON.parse(m[1]) : {};
  const lookup = (key) => {
    const v = key.split('.').reduce((o, x) => (o && typeof o === 'object' && x in o ? o[x] : undefined), EN);
    return typeof v === 'string' ? v : undefined;
  };
  const t = (key, params) => {
    const p = params || {};
    let k = key;
    if (typeof p.count === 'number') {
      const form = new Intl.PluralRules('en').select(p.count);
      k = lookup(key + '_' + form) !== undefined ? key + '_' + form : key + '_other';
    }
    const s = lookup(k);
    if (s === undefined) return key;
    return s.replace(/\{\{\s*(\w+)\s*\}\}/g, (all, name) => (Object.prototype.hasOwnProperty.call(p, name) ? String(p[name]) : all));
  };
  const setT = (key, fb) => (lookup(key) !== undefined ? t(key) : fb);
  return { t, setT, i18nString: lookup, EN };
}
