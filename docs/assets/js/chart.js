/* Interactive results chart (vanilla SVG, no dependencies).
   Data: window.CASDWN_RESULTS[metric][packetSize][priority][mechanism] = [[devices, mean, ci95], ...] */
(function () {
  'use strict';

  var DATA = window.CASDWN_RESULTS;
  var root = document.getElementById('viz');
  if (!DATA || !root) return;

  var SVGNS = 'http://www.w3.org/2000/svg';
  var reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

  // Legend order = reading order; draw order is reversed so IA SDWN sits on top.
  var SERIES = [
    { k: 'ia',   name: 'IA SDWN',     token: '--s-ia',   marker: 'circle',   ours: true },
    { k: 'mod',  name: 'Mod. SDWN',   token: '--s-mod',  marker: 'square' },
    { k: 'orig', name: 'Orig. SDWN',  token: '--s-orig', marker: 'triangle' },
    { k: 'be',   name: 'Best effort', token: '--s-be',   marker: 'diamond' }
  ];
  var METRICS = {
    delay:      { label: 'Delay',       unit: 'ms',   axis: 'Mean delay (ms)',       digits: 3, zero: true },
    loss:       { label: 'Packet loss', unit: '%',    axis: 'Packet loss (%)',       digits: 3, zero: true },
    throughput: { label: 'Throughput',  unit: 'kbps', axis: 'Throughput (kbps)',     digits: 3, zero: false }
  };
  var PRIOS = { H: 'High priority', M: 'Medium priority', L: 'Low priority', NRT: 'Non-real-time' };

  var state = { metric: 'delay', prio: 'H', size: '1024' };
  var hidden = {};
  var hoverIndex = null;
  var firstDraw = true;

  var chartEl = document.getElementById('chart');
  var tipEl = document.getElementById('chart-tip');
  var titleEl = document.getElementById('viz-title');
  var legendEl = document.getElementById('legend');

  chartEl.tabIndex = 0;
  chartEl.setAttribute('role', 'img');

  /* ---------- helpers ---------- */
  function el(name, attrs, parent) {
    var n = document.createElementNS(SVGNS, name);
    for (var a in attrs) n.setAttribute(a, attrs[a]);
    if (parent) parent.appendChild(n);
    return n;
  }
  function cssVar(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
  function fmt(v, d) { return v.toLocaleString('en-US', { minimumFractionDigits: d, maximumFractionDigits: d }); }

  function niceStep(span, count) {
    var raw = span / Math.max(1, count);
    var mag = Math.pow(10, Math.floor(Math.log10(raw)));
    var n = raw / mag;
    return (n >= 5 ? 10 : n >= 2 ? 5 : n >= 1 ? 2 : 1) * mag;
  }
  function niceDomain(lo, hi, count) {
    if (hi === lo) { hi = lo + 1; }
    var step = niceStep(hi - lo, count);
    return { lo: Math.floor(lo / step) * step, hi: Math.ceil(hi / step) * step, step: step };
  }

  function markerPath(type, x, y, r) {
    switch (type) {
      case 'square':   return 'M' + (x - r * .85) + ' ' + (y - r * .85) + 'h' + r * 1.7 + 'v' + r * 1.7 + 'h' + (-r * 1.7) + 'Z';
      case 'triangle': return 'M' + x + ' ' + (y - r * 1.05) + 'L' + (x + r) + ' ' + (y + r * .75) + 'L' + (x - r) + ' ' + (y + r * .75) + 'Z';
      case 'diamond':  return 'M' + x + ' ' + (y - r * 1.15) + 'L' + (x + r * 1.15) + ' ' + y + 'L' + x + ' ' + (y + r * 1.15) + 'L' + (x - r * 1.15) + ' ' + y + 'Z';
      default:         return 'M' + (x - r) + ' ' + y + 'a' + r + ' ' + r + ' 0 1 0 ' + 2 * r + ' 0a' + r + ' ' + r + ' 0 1 0 ' + (-2 * r) + ' 0';
    }
  }

  function current() { return DATA[state.metric][state.size][state.prio]; }
  function visibleSeries() { return SERIES.filter(function (s) { return !hidden[s.k]; }); }

  /* ---------- legend ---------- */
  function buildLegend() {
    legendEl.textContent = '';
    SERIES.forEach(function (s) {
      var b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('aria-pressed', hidden[s.k] ? 'false' : 'true');
      b.dataset.k = s.k;
      var sw = el('svg', { viewBox: '0 0 22 12', 'aria-hidden': 'true' });
      el('line', { x1: 1, y1: 6, x2: 21, y2: 6, stroke: cssVar(s.token), 'stroke-width': s.ours ? 2.75 : 2, 'stroke-linecap': 'round' }, sw);
      el('path', { d: markerPath(s.marker, 11, 6, 3.6), fill: cssVar(s.token), stroke: cssVar('--surface'), 'stroke-width': 1.5 }, sw);
      b.appendChild(sw);
      b.appendChild(document.createTextNode(s.name));
      b.addEventListener('click', function () {
        if (!hidden[s.k] && visibleSeries().length === 1) return; // keep at least one
        hidden[s.k] = !hidden[s.k];
        buildLegend();
        draw();
      });
      b.addEventListener('mouseenter', function () { emphasize(s.k); });
      b.addEventListener('mouseleave', function () { emphasize(null); });
      b.addEventListener('focus', function () { emphasize(s.k); });
      b.addEventListener('blur', function () { emphasize(null); });
      legendEl.appendChild(b);
    });
  }

  function emphasize(k) {
    chartEl.querySelectorAll('.series').forEach(function (g) {
      g.classList.toggle('dim', !!k && g.dataset.k !== k);
    });
  }

  /* ---------- chart ---------- */
  var geo = null;

  function draw() {
    var m = METRICS[state.metric];
    var data = current();
    var series = visibleSeries();
    titleEl.textContent = m.label + ' · ' + PRIOS[state.prio] + ' · ' + state.size + ' B packets';

    var W = chartEl.clientWidth, H = chartEl.clientHeight;
    if (W < 160 || H < 120) return; // not laid out yet; ResizeObserver will call again
    var compact = W < 560;
    var pad = { t: 14, r: compact ? 12 : 104, b: 46, l: 58 };
    var iw = W - pad.l - pad.r, ih = H - pad.t - pad.b;
    if (iw < 80 || ih < 80) return;

    var xs = data.ia.map(function (p) { return p[0]; });
    var lo = Infinity, hi = -Infinity;
    series.forEach(function (s) {
      data[s.k].forEach(function (p) { lo = Math.min(lo, p[1] - p[2]); hi = Math.max(hi, p[1] + p[2]); });
    });
    if (m.zero) lo = 0;
    var dom = niceDomain(lo, hi, compact ? 4 : 5);

    var x = function (v) { return pad.l + (v - xs[0]) / (xs[xs.length - 1] - xs[0]) * iw; };
    var y = function (v) { return pad.t + ih - (v - dom.lo) / (dom.hi - dom.lo) * ih; };
    geo = { x: x, y: y, xs: xs, pad: pad, iw: iw, ih: ih, W: W, H: H };

    var svg = el('svg', { viewBox: '0 0 ' + W + ' ' + H, 'aria-hidden': 'true', focusable: 'false' });

    // grid + y axis
    var grid = el('g', { class: 'grid' }, svg);
    var axis = el('g', { class: 'axis' }, svg);
    var dYdigits = dom.step < 0.01 ? 3 : dom.step < 0.1 ? 2 : dom.step < 1 ? 1 : 0;
    for (var v = dom.lo; v <= dom.hi + dom.step / 2; v += dom.step) {
      var yy = Math.round(y(v)) + .5;
      if (Math.abs(v - dom.lo) > 1e-9) el('line', { x1: pad.l, x2: pad.l + iw, y1: yy, y2: yy }, grid);
      var t = el('text', { x: pad.l - 10, y: yy + 4, 'text-anchor': 'end' }, axis);
      t.textContent = fmt(v, dYdigits);
    }
    el('line', { class: 'baseline', x1: pad.l, x2: pad.l + iw, y1: pad.t + ih + .5, y2: pad.t + ih + .5 }, svg);

    // x axis
    xs.forEach(function (d, i) {
      if (compact && i % 2 === 1 && i !== xs.length - 1) return;
      var t = el('text', { x: x(d), y: pad.t + ih + 20, 'text-anchor': 'middle' }, axis);
      t.textContent = d;
    });
    var xt = el('text', { class: 'axis-title', x: pad.l + iw / 2, y: H - 6, 'text-anchor': 'middle' }, svg);
    xt.textContent = 'Devices in the network';
    var yt = el('text', { class: 'axis-title', transform: 'translate(14 ' + (pad.t + ih / 2) + ') rotate(-90)', 'text-anchor': 'middle' }, svg);
    yt.textContent = m.axis;

    // series (draw back-to-front so IA SDWN is on top)
    var lines = [];
    series.slice().reverse().forEach(function (s) {
      var pts = data[s.k];
      var color = cssVar(s.token);
      var g = el('g', { class: 'series', 'data-k': s.k }, svg);

      var top = pts.map(function (p) { return x(p[0]) + ',' + y(Math.min(dom.hi, p[1] + p[2])); });
      var bot = pts.slice().reverse().map(function (p) { return x(p[0]) + ',' + y(Math.max(dom.lo, p[1] - p[2])); });
      el('polygon', { class: 'band', points: top.concat(bot).join(' '), fill: color }, g);

      var d = pts.map(function (p, i) { return (i ? 'L' : 'M') + x(p[0]).toFixed(1) + ' ' + y(p[1]).toFixed(1); }).join('');
      var path = el('path', { class: 'line' + (s.ours ? ' is-ours' : ''), d: d, stroke: color }, g);
      lines.push(path);

      pts.forEach(function (p) {
        el('path', { class: 'dot', d: markerPath(s.marker, x(p[0]), y(p[1]), compact ? 3.4 : 4), fill: color }, g);
      });
    });

    // direct end labels (de-collided)
    if (!compact) {
      var labels = series.map(function (s) {
        var last = data[s.k][data[s.k].length - 1];
        return { s: s, y: y(last[1]) };
      }).sort(function (a, b) { return a.y - b.y; });
      var gap = 15;
      for (var i = 1; i < labels.length; i++) if (labels[i].y - labels[i - 1].y < gap) labels[i].y = labels[i - 1].y + gap;
      var overflow = labels.length ? labels[labels.length - 1].y - (pad.t + ih) : 0;
      if (overflow > 0) labels.forEach(function (l) { l.y -= overflow; });
      labels.forEach(function (l) {
        var t = el('text', { class: 'end-label', x: pad.l + iw + 10, y: l.y + 4 }, svg);
        t.textContent = l.s.name;
      });
    }

    // hover layer
    var hover = el('g', { class: 'hover-layer', visibility: 'hidden' }, svg);
    el('line', { class: 'crosshair', y1: pad.t, y2: pad.t + ih }, hover);
    series.forEach(function (s) { el('circle', { class: 'hover-dot', r: 5.5, fill: cssVar(s.token), 'data-k': s.k }, hover); });
    var hit = el('rect', { x: pad.l - 12, y: pad.t, width: iw + 24, height: ih, fill: 'transparent' }, svg);

    function idxFromEvent(ev) {
      var r = svg.getBoundingClientRect();
      var px = (ev.clientX - r.left) * (W / r.width);
      var best = 0, bd = Infinity;
      xs.forEach(function (d, i) { var dd = Math.abs(x(d) - px); if (dd < bd) { bd = dd; best = i; } });
      return best;
    }
    hit.addEventListener('pointermove', function (ev) { showHover(idxFromEvent(ev)); });
    hit.addEventListener('pointerdown', function (ev) { showHover(idxFromEvent(ev)); });
    hit.addEventListener('pointerleave', function (ev) { if (ev.pointerType === 'mouse') hideHover(); });

    chartEl.querySelectorAll('svg').forEach(function (n) { n.remove(); });
    chartEl.appendChild(svg);
    chartEl.setAttribute('aria-label', titleEl.textContent + '. Line chart of ' + series.map(function (s) { return s.name; }).join(', ') +
      ' from 10 to 100 devices. Use left and right arrow keys to read the values.');

    if (firstDraw && !reduceMotion.matches && 'animate' in Element.prototype) {
      lines.forEach(function (p, i) {
        var len = p.getTotalLength();
        p.style.strokeDasharray = len;
        p.animate([{ strokeDashoffset: len }, { strokeDashoffset: 0 }], { duration: 900, delay: 120 * i, easing: 'cubic-bezier(.2,.7,.2,1)', fill: 'backwards' })
          .onfinish = function () { p.style.strokeDasharray = ''; };
      });
    }
    firstDraw = false;

    if (hoverIndex !== null) showHover(hoverIndex); else hideHover();
  }

  function showHover(i) {
    if (!geo) return;
    hoverIndex = i;
    var data = current();
    var m = METRICS[state.metric];
    var svg = chartEl.querySelector('svg');
    var layer = svg.querySelector('.hover-layer');
    var cx = geo.x(geo.xs[i]);
    layer.setAttribute('visibility', 'visible');
    var ch = layer.querySelector('.crosshair');
    ch.setAttribute('x1', cx); ch.setAttribute('x2', cx);
    layer.querySelectorAll('.hover-dot').forEach(function (c) {
      c.setAttribute('cx', cx);
      c.setAttribute('cy', geo.y(data[c.dataset.k][i][1]));
    });

    var rows = visibleSeries().map(function (s) {
      var p = data[s.k][i];
      return '<div class="tt-r"><i style="background:' + cssVar(s.token) + '"></i><span>' + s.name + '</span><span><b>' +
        fmt(p[1], m.digits) + '</b> <small>± ' + fmt(p[2], m.digits) + '</small></span></div>';
    }).join('');
    tipEl.innerHTML = '<div class="tt-h">' + geo.xs[i] + ' devices · ' + m.unit + '</div>' + rows;
    tipEl.hidden = false;

    // position relative to .viz
    var vr = root.getBoundingClientRect(), cr = chartEl.getBoundingClientRect();
    var scale = cr.width / geo.W;
    var left = cr.left - vr.left + cx * scale;
    var tw = tipEl.offsetWidth;
    var place = left + 16 + tw > vr.width - 8 ? left - 16 - tw : left + 16;
    tipEl.style.left = Math.max(8, place) + 'px';
    tipEl.style.top = (cr.top - vr.top + geo.pad.t * scale + 4) + 'px';
  }

  function hideHover() {
    hoverIndex = null;
    var layer = chartEl.querySelector('.hover-layer');
    if (layer) layer.setAttribute('visibility', 'hidden');
    tipEl.hidden = true;
  }

  chartEl.addEventListener('keydown', function (e) {
    if (!geo) return;
    var n = geo.xs.length;
    if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
      e.preventDefault();
      var i = hoverIndex === null ? (e.key === 'ArrowRight' ? 0 : n - 1) : Math.min(n - 1, Math.max(0, hoverIndex + (e.key === 'ArrowRight' ? 1 : -1)));
      showHover(i);
    } else if (e.key === 'Escape') hideHover();
  });
  chartEl.addEventListener('blur', hideHover);
  document.addEventListener('pointerdown', function (e) { if (!chartEl.contains(e.target)) hideHover(); });

  /* ---------- filters (radiogroups) ---------- */
  root.querySelectorAll('.seg').forEach(function (group) {
    var key = group.dataset.key;
    var buttons = Array.prototype.slice.call(group.querySelectorAll('button'));
    function sync() {
      buttons.forEach(function (b) {
        var on = b.dataset.v === state[key];
        b.setAttribute('aria-checked', String(on));
        b.tabIndex = on ? 0 : -1;
      });
    }
    buttons.forEach(function (b, i) {
      b.addEventListener('click', function () { state[key] = b.dataset.v; sync(); draw(); });
      b.addEventListener('keydown', function (e) {
        var d = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
        if (!d) return;
        e.preventDefault();
        var nb = buttons[(i + d + buttons.length) % buttons.length];
        nb.focus(); nb.click();
      });
    });
    sync();
  });

  /* ---------- lifecycle ---------- */
  var raf = 0;
  function redraw() { cancelAnimationFrame(raf); raf = requestAnimationFrame(function () { buildLegend(); draw(); }); }
  function onResize() {
    // ignore the observer's initial callback (and no-op resizes) so the first draw animation isn't cut short
    if (geo && geo.W === chartEl.clientWidth && geo.H === chartEl.clientHeight) return;
    redraw();
  }
  if ('ResizeObserver' in window) new ResizeObserver(onResize).observe(chartEl); else window.addEventListener('resize', onResize);

  buildLegend();
  draw();
})();
