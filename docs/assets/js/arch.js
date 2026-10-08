/* Animated CA-SDWN architecture: data packets over WiFi + the closed control loop.
   Continuous motion uses SVG SMIL (packets) and CSS (devices, gear, queues);
   the closed-loop story is a small timeline that also highlights the 5 loop cards. */
(function () {
  'use strict';

  var svg = document.getElementById('arch');
  if (!svg) return;
  var NS = 'http://www.w3.org/2000/svg';
  var statusEl = document.getElementById('arch-status');
  var steps = document.querySelectorAll('.loop-step[data-step]');
  var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  if (reduce) { svg.dataset.joined = '1'; svg.classList.add('static'); return; } // static diagram, everything visible
  svg.dataset.joined = '0'; // the first event of the story is a new sensor joining

  function el(name, attrs, parent) {
    var n = document.createElementNS(NS, name);
    for (var k in attrs) n.setAttribute(k, attrs[k]);
    if (parent) parent.appendChild(n);
    return n;
  }
  function motion(parent, pathId, attrs) {
    var m = el('animateMotion', attrs, parent);
    el('mpath', { href: '#' + pathId }, m);
    return m;
  }

  /* ---------- continuous uplink packets (devices → AP) ---------- */
  var PRIO = ['H', 'M', 'H', 'L', 'N', 'M'];
  var packets = document.getElementById('packets');
  PRIO.forEach(function (p, i) {
    var g = el('g', i === 5 ? { class: 'joinable' } : {}, packets);
    for (var k = 0; k < 2; k++) {
      var c = el('circle', { r: 4.5, class: 'pk pk-' + p, opacity: 0 }, g);
      var dur = (2.2 + i * 0.12).toFixed(2) + 's';
      var begin = (i * 0.37 + k * 1.15).toFixed(2) + 's';
      motion(c, 'up' + (i + 1), { dur: dur, begin: begin, repeatCount: 'indefinite', rotate: 'auto' });
      el('animate', { attributeName: 'opacity', values: '0;1;1;0', keyTimes: '0;0.1;0.85;1', dur: dur, begin: begin, repeatCount: 'indefinite' }, c);
    }
  });

  /* ---------- one-shot control tokens ---------- */
  function token(pathId, ms, cls, reverse) {
    var c = el('circle', { r: 7, class: 'token ' + (cls || ''), opacity: 0, filter: 'url(#glow)' }, packets);
    var dur = ms / 1000 + 's';
    var attrs = { dur: dur, begin: 'indefinite', fill: 'freeze' };
    if (reverse) { attrs.keyPoints = '1;0'; attrs.keyTimes = '0;1'; attrs.calcMode = 'linear'; }
    var m = motion(c, pathId, attrs);
    var a = el('animate', { attributeName: 'opacity', values: '0;1;1;0', keyTimes: '0;0.1;0.85;1', dur: dur, begin: 'indefinite', fill: 'freeze' }, c);
    m.beginElement(); a.beginElement();
    var path = document.getElementById(pathId);
    if (path.closest('.ctrl-links')) path.classList.add('flow');
    setTimeout(function () { c.remove(); path.classList.remove('flow'); }, ms + 80);
  }

  /* ---------- timeline helpers (pause-aware) ---------- */
  var paused = false;
  function sleep(ms) {
    return new Promise(function (resolve) {
      var left = ms, last = performance.now();
      (function tick() {
        var now = performance.now();
        if (!paused) left -= now - last;
        last = now;
        if (left <= 0) resolve(); else setTimeout(tick, Math.min(100, Math.max(16, left)));
      })();
    });
  }
  function setPhase(n, text) {
    svg.dataset.phase = String(n);
    statusEl.style.opacity = '0';
    setTimeout(function () { statusEl.textContent = text; statusEl.style.opacity = '1'; }, 200);
    steps.forEach(function (s) { s.classList.toggle('is-active', s.dataset.step === String(n)); });
  }
  function $(sel) { return svg.querySelector(sel); }
  function $$(sel) { return Array.prototype.slice.call(svg.querySelectorAll(sel)); }

  var ap = $('#ap'), cap = $('#cap'), ctrlBox = $('#ctrl .box'), ctrl = $('#ctrl');
  var boSearch = document.getElementById('bo-search');

  function reset() {
    $$('.tn, .outs rect').forEach(function (n) { n.classList.remove('lit'); });
    svg.classList.remove('bo-run', 'bo-found', 'bcast');
    ap.classList.remove('active'); cap.classList.remove('active'); ctrlBox.classList.remove('active'); ctrl.classList.remove('busy');
    $$('.dev').forEach(function (d) { d.classList.remove('flash'); });
  }

  async function run() {
    setPhase(0, 'IoT sensors and actuators send their data over WiFi');
    await sleep(2600);

    for (;;) {
      // 1 · Observe: a node joins or leaves → the AP reports the new context
      var joining = svg.dataset.joined === '0';
      svg.dataset.joined = joining ? '1' : '0';
      setPhase(1, joining ? 'Observe · a new sensor joins the network' : 'Observe · a sensor leaves the network');
      ap.classList.add('active');
      await sleep(700);
      token('p-req', 1200);
      await sleep(1300);
      ap.classList.remove('active');

      // 2 · Predict: controller asks the agent, the Random Forest predicts D, T, L
      setPhase(2, 'Predict · inside the CAP controller, the Random Forest estimates delay, throughput and loss');
      cap.classList.add('active'); ctrlBox.classList.add('active'); ctrl.classList.add('busy');
      token('p-ask', 600);
      await sleep(650);
      for (var lvl = 0; lvl < 3; lvl++) {
        $$('.tn[data-o="' + lvl + '"]').forEach(function (n) { n.classList.add('lit'); });
        await sleep(280);
      }
      var outs = $$('.outs rect');
      for (var i = 0; i < outs.length; i++) { outs[i].classList.add('lit'); await sleep(220); }
      await sleep(700);

      // 3 · Optimize: Bayesian optimization searches the best CWmin/CWmax
      setPhase(3, 'Optimize · Bayesian optimization finds the best CWmin / CWmax');
      svg.classList.add('bo-run');
      boSearch.beginElement();
      await sleep(2300);
      svg.classList.add('bo-found');
      await sleep(900);

      // 4 · Reconfigure: agent → controller → AP → every node
      setPhase(4, 'Reconfigure · the CAP controller sends the new parameters to the access point and every node');
      token('p-cw', 650, 'cw');
      await sleep(700);
      token('p-upd', 1200, 'cw');
      await sleep(1250);
      cap.classList.remove('active');
      ap.classList.add('active');
      svg.classList.add('bcast');
      ['up1', 'up2', 'up3', 'up4', 'up5', 'up6'].forEach(function (id, k) {
        if (id === 'up6' && svg.dataset.joined === '0') return;
        token(id, 900, 'cw', true);
      });
      await sleep(850);
      $$('.dev').forEach(function (d) { d.classList.add('flash'); });
      await sleep(1100);

      // 5 · Repeat: wait for the next context change
      setPhase(5, 'Repeat · every context change triggers a new cycle');
      await sleep(1600);
      reset();
      await sleep(400);
    }
  }

  /* ---------- pause when off-screen or tab hidden ---------- */
  var visible = false;
  function updatePause() {
    paused = !visible || document.hidden;
    svg.classList.toggle('paused', paused);
    if (paused) svg.pauseAnimations(); else svg.unpauseAnimations();
  }
  if ('IntersectionObserver' in window) {
    new IntersectionObserver(function (entries) {
      visible = entries[0].isIntersecting; updatePause();
    }, { threshold: 0.15 }).observe(svg);
  } else { visible = true; }
  document.addEventListener('visibilitychange', updatePause);
  updatePause();

  run();
})();
