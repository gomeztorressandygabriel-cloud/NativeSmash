/* NativeSmash's players server (Supabase, server/supabase/schema.sql) for the web pages: the project, the language switch, the online
   count and the ranking's divisions. The URL and the publishable key are public by design; the tables are closed and reached through
   the server's functions. */
window.NS = (function () {
  var URL_ = 'https://vsaogrjmlwzwvdtffyow.supabase.co';
  var KEY = 'sb_publishable_ia8FosvjKAWFkLkMf4lXVA_uJmPZFg4';
  /* the community: the official WhatsApp group, and the Discord server's invitation (empty: its buttons stay hidden) */
  var LINKS = { whatsapp: 'https://chat.whatsapp.com/CuV6VkBEa8Y6yVvsEcREY2', discord: '' };
  function showLinks() {
    document.querySelectorAll('[data-whatsapp]').forEach(function (a) { a.href = LINKS.whatsapp; a.hidden = !LINKS.whatsapp; });
    document.querySelectorAll('[data-discord]').forEach(function (a) { a.href = LINKS.discord || '#'; a.hidden = !LINKS.discord; });
  }
  var client = window.supabase ? window.supabase.createClient(URL_, KEY, { auth: { persistSession: true, detectSessionInUrl: true } }) : null;

  function lang() { return document.documentElement.lang === 'en' ? 'en' : 'es'; }
  function setLang(l) {
    document.documentElement.lang = l;
    document.querySelectorAll('.lang button').forEach(function (b) { b.setAttribute('aria-pressed', b.dataset.set === l ? 'true' : 'false'); });
    try { localStorage.setItem('ns-lang', l); } catch (e) {}
    document.dispatchEvent(new CustomEvent('ns-lang'));
  }
  function initLang() {
    document.querySelectorAll('.lang button').forEach(function (b) { b.addEventListener('click', function () { setLang(b.dataset.set); }); });
    var saved = null;
    try { saved = localStorage.getItem('ns-lang'); } catch (e) {}
    if (saved === 'en' || saved === 'es') setLang(saved);
    else if (!/^es/i.test(navigator.language || 'es')) setLang('en');
  }

  /* how many have the launcher open, into every [data-online] (the number) and shows [data-online-box] */
  function showOnline() {
    fetch(URL_ + '/rest/v1/rpc/online_count', { method: 'POST', headers: { apikey: KEY, 'Content-Type': 'application/json' }, body: '{}' })
      .then(function (r) { return r.ok ? r.json() : Promise.reject(); })
      .then(function (n) {
        document.querySelectorAll('[data-online]').forEach(function (el) { el.textContent = String(n); });
        document.querySelectorAll('[data-online-box]').forEach(function (el) { el.hidden = false; });
      })
      .catch(function () {});
  }

  /* the ranking's divisions, by rating (everyone starts at 1000) */
  var TIERS = [
    { min: 1700, es: 'Aspirante', en: 'Challenger', color: '#ffd36b' },
    { min: 1550, es: 'Maestro', en: 'Master', color: '#c58bff' },
    { min: 1400, es: 'Diamante', en: 'Diamond', color: '#7cc7ff' },
    { min: 1300, es: 'Esmeralda', en: 'Emerald', color: '#3ddc97' },
    { min: 1200, es: 'Platino', en: 'Platinum', color: '#5fd6c9' },
    { min: 1100, es: 'Oro', en: 'Gold', color: '#f1c15b' },
    { min: 1000, es: 'Plata', en: 'Silver', color: '#c4ccdc' },
    { min: 900, es: 'Bronce', en: 'Bronze', color: '#d08a5c' },
    { min: -1e9, es: 'Hierro', en: 'Iron', color: '#8e8a93' }
  ];
  function tier(rating) { for (var i = 0; i < TIERS.length; i++) if (rating >= TIERS[i].min) return TIERS[i]; return TIERS[TIERS.length - 1]; }

  function esc(s) { return String(s).replace(/[&<>"']/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]; }); }

  return { links: LINKS, showLinks: showLinks, client: client, url: URL_, key: KEY, lang: lang, initLang: initLang, showOnline: showOnline, TIERS: TIERS, tier: tier, esc: esc };
})();
