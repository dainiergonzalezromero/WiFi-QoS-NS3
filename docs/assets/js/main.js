/* Mobile menu and BibTeX copy button. */
(function () {
  'use strict';

  var btn = document.getElementById('menu-toggle');
  var menu = document.getElementById('mobile-menu');
  var icon = document.getElementById('menu-icon');
  function setMenu(open) {
    menu.classList.toggle('hidden', !open);
    icon.classList.toggle('fa-bars', !open);
    icon.classList.toggle('fa-xmark', open);
    btn.setAttribute('aria-expanded', String(open));
    btn.setAttribute('aria-label', open ? 'Close menu' : 'Open menu');
  }
  btn.addEventListener('click', function () { setMenu(menu.classList.contains('hidden')); });
  menu.addEventListener('click', function (e) { if (e.target.closest('a')) setMenu(false); });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') setMenu(false); });

  /* hero parallax: expose scroll progress (0 → 1) as --p on the hero */
  var hero = document.getElementById('hero');
  var heroContent = hero && hero.querySelector('.hero-content');
  if (hero && !window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    // once the entrance fade ends, let the scroll effect drive opacity
    heroContent.addEventListener('animationend', function (e) {
      if (e.target !== heroContent) return; // ignore animations of child elements
      heroContent.classList.remove('animate-fade-in-up');
      heroContent.classList.add('entered');
    });
    var ticking = false;
    var update = function () {
      ticking = false;
      var h = hero.offsetHeight || 1;
      var p = Math.min(1, Math.max(0, window.scrollY / h));
      hero.style.setProperty('--p', p.toFixed(4));
    };
    window.addEventListener('scroll', function () {
      if (!ticking) { ticking = true; requestAnimationFrame(update); }
    }, { passive: true });
    update();
  }

  document.querySelectorAll('[data-copy]').forEach(function (b) {
    b.setAttribute('aria-live', 'polite');
    b.addEventListener('click', function () {
      var text = document.getElementById(b.dataset.copy).innerText.trim();
      var done = function () { b.textContent = 'Copied!'; setTimeout(function () { b.textContent = 'Copy'; }, 1600); };
      if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text).then(done);
      } else {
        var ta = document.createElement('textarea');
        ta.value = text; ta.style.position = 'fixed'; ta.style.opacity = '0';
        document.body.appendChild(ta); ta.select();
        try { document.execCommand('copy'); done(); } catch (e) { /* text stays selectable */ }
        ta.remove();
      }
    });
  });
})();
