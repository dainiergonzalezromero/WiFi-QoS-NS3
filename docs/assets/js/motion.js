/* Scroll reveals and count-up numbers with Motion (motion.dev).
   Progressive enhancement: without this file (or with reduced motion) everything
   is visible and the numbers show their final values. */
const reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

if (!reduce) {
  try {
    const { animate, inView, stagger } = await import('https://cdn.jsdelivr.net/npm/motion@12.43.0/+esm');
    const ease = [0.16, 1, 0.3, 1];
    const root = document.documentElement;

    // Reveal blocks that are still below the fold; anything already on screen stays put.
    const pending = [...document.querySelectorAll('.reveal')].filter(
      (el) => el.getBoundingClientRect().top > window.innerHeight * 0.92
    );
    pending.forEach((el) => el.classList.add('is-pending'));
    root.classList.add('motion-on');

    inView('.reveal.is-pending', (el) => {
      const siblings = [...el.parentElement.children].filter((c) => c.classList.contains('is-pending'));
      const i = Math.max(0, siblings.indexOf(el));
      el.style.opacity = '0';
      el.classList.remove('is-pending');
      animate(el, { opacity: [0, 1], y: [60, 0] }, { duration: 0.9, delay: Math.min(i, 5) * 0.1, ease })
        .then(() => { el.style.opacity = ''; el.style.transform = ''; }); // hand transform back to CSS (hover lift)
    }, { margin: '0px 0px -8% 0px' });

    // Count-up numbers
    const nf = new Intl.NumberFormat('en-US');
    inView('[data-count]', (el) => {
      const target = Number(el.dataset.count);
      animate(0, target, {
        duration: target > 1000 ? 2 : 1.4,
        ease,
        onUpdate: (v) => { el.textContent = nf.format(Math.round(v)); },
      });
    }, { amount: 0.6 });
  } catch (err) {
    document.documentElement.classList.remove('motion-on');
  }
}
