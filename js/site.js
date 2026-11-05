/* 番茄小说网作家后台自动化脚本
 * 设计原则：不依赖 class 名（番茄用的是构建后随机化的类名），
 * 一律按「可见文本 / placeholder / 结构位置」定位，并且每步失败都回传
 * 当前页面的按钮与输入框清单，方便第一次运行时据此修正。
 */
(function () {
  const FQ = {};
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const T = (el) => ((el && (el.innerText || el.textContent)) || '').replace(/\s+/g, ' ').trim();
  const norm = (s) => (s || '').replace(/[\s\u3000\u00a0]/g, '');
  const area = (el) => {
    const r = el.getBoundingClientRect();
    return r.width * r.height;
  };

  function vis(el, allowHidden) {
    if (!el || !el.getBoundingClientRect) return false;
    const r = el.getBoundingClientRect();
    if (r.width < 1 || r.height < 1) return false;
    const st = getComputedStyle(el);
    if (st.display === 'none' || st.visibility === 'hidden') return false;
    if (!allowHidden && parseFloat(st.opacity || '1') < 0.05) return false;
    return true;
  }

  // 只认「最内层」元素：子元素文字和自己一样就不算，避免点到整块容器
  function inner(el) {
    for (const c of el.children) if (T(c) === T(el)) return false;
    return true;
  }

  function pool(allowHidden) {
    const sel = 'button,a,div,span,p,li,label,td,[role="button"],[role="option"],[role="radio"],h1,h2,h3,h4';
    return [...document.querySelectorAll(sel)].filter((e) => vis(e, allowHidden) && inner(e));
  }

  function isDisabled(el) {
    let cur = el;
    for (let i = 0; i < 4 && cur; i++) {
      if (cur.disabled === true) return true;
      if (cur.getAttribute && cur.getAttribute('aria-disabled') === 'true') return true;
      const cls = (cur.className || '') + '';
      if (/\bdisabled\b|is-disabled|-disabled/i.test(cls)) return true;
      cur = cur.parentElement;
    }
    return false;
  }

  function findByText(texts, opts) {
    opts = opts || {};
    const want = Array.isArray(texts) ? texts : [texts];
    const list = pool(opts.allowHidden);
    let best = null;
    for (const el of list) {
      if (opts.skipDisabled && isDisabled(el)) continue;
      const t = T(el);
      if (!t) continue;
      for (const w of want) {
        const hit = opts.exact ? t === w : t === w || t.indexOf(w) >= 0;
        if (!hit) continue;
        const r = el.getBoundingClientRect();
        if (r.width > (opts.maxW || 460) || r.height > (opts.maxH || 140)) continue;
        const a = r.width * r.height;
        if (!best || a < best.area) best = { el, area: a, text: t };
      }
    }
    return best;
  }

  // 诊断用：失败时把页面上能点的东西带回去
  function buttons(limit) {
    const out = [];
    for (const el of pool(false)) {
      const t = T(el);
      if (!t || t.length > 24) continue;
      if (out.indexOf(t) >= 0) continue;
      out.push(t);
      if (out.length >= (limit || 40)) break;
    }
    return out;
  }

  function inputs() {
    return [...document.querySelectorAll('input,textarea')]
      .filter((e) => vis(e))
      .slice(0, 12)
      .map((e) => ({
        ph: e.getAttribute('placeholder') || '',
        type: e.getAttribute('type') || e.tagName.toLowerCase(),
        val: e.value || '',
      }));
  }

  function clickText(texts, opts) {
    const hit = findByText(texts, opts);
    if (!hit) return { ok: false, why: 'not-found', buttons: buttons() };
    hit.el.scrollIntoView({ block: 'center' });
    hit.el.click();
    return { ok: true, text: hit.text };
  }

  // React 受控组件必须走原生 setter + input 事件，直接改 value 不生效
  function setValue(el, val) {
    const proto = el.tagName === 'TEXTAREA'
      ? window.HTMLTextAreaElement.prototype
      : window.HTMLInputElement.prototype;
    const desc = Object.getOwnPropertyDescriptor(proto, 'value');
    el.focus();
    if (desc && desc.set) desc.set.call(el, val);
    else el.value = val;
    el.dispatchEvent(new Event('input', { bubbles: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
    el.dispatchEvent(new Event('blur', { bubbles: true }));
    return el.value;
  }

  function editorEl() {
    const cands = [...document.querySelectorAll('[contenteditable="true"],.ProseMirror,textarea')]
      .filter((e) => vis(e) && !e.closest('[contenteditable="false"]'));
    if (!cands.length) return null;
    cands.sort((a, b) => area(b) - area(a));
    return cands[0];
  }

  // ---------- 页面状态 ----------

  FQ.ready = function () {
    return { url: location.href, title: document.title, hasBody: !!document.body };
  };

  // 是否已经在某个写作/管理页
  // 注意：书籍列表页的书卡上也有「章节管理」四个字，不能拿它当判据，
  // 只有出现「新建章节」按钮或分卷下拉才算真的在章节管理页。
  FQ.stage = function () {
    const t = T(document.body);
    const hasNew = !!findByText(['新建章节'], { maxW: 280, maxH: 90 });
    const hasVol = pool(false).some((e) => isVol(T(e)));
    const hasTitle = !!document.querySelector('input[placeholder*="标题"],textarea[placeholder*="标题"]');
    const hasPub = t.indexOf('发布设置') >= 0;
    const hasCorrect = t.indexOf('智能纠错') >= 0;
    const manage = hasNew || hasVol;
    return {
      url: location.href,
      newChapter: hasNew,
      volume: hasVol,
      editor: hasTitle,
      publish: hasPub,
      correct: hasCorrect,
      manage: manage,
      books: /book-manage/.test(location.href) && !manage,
    };
  };

  FQ.probe = function () {
    return {
      url: location.href,
      title: document.title,
      stage: FQ.stage(),
      buttons: buttons(60),
      inputs: inputs(),
      hasEditor: !!editorEl(),
      bodySample: T(document.body).slice(0, 300),
    };
  };

  // ---------- 书籍管理页 ----------

  FQ.books = function () {
    const out = [];
    const seen = {};
    for (const e of pool(false)) {
      const t = T(e);
      if (!t || t.length < 2 || t.length > 40) continue;
      const st = getComputedStyle(e);
      const fs = parseFloat(st.fontSize) || 0;
      const fw = parseInt(st.fontWeight) || 400;
      if (fs < 13 || fw < 500) continue;
      let p = e;
      let ok = false;
      for (let i = 0; i < 8 && p; i++) {
        const pt = T(p);
        if (pt.indexOf('章节管理') >= 0 || /已发布|连载模式|完本模式|签约/.test(pt)) {
          ok = true;
          break;
        }
        p = p.parentElement;
      }
      if (!ok) continue;
      if (seen[t]) continue;
      seen[t] = 1;
      out.push(t);
    }
    return out;
  };

  // 鼠标移到书卡上，「章节管理」按钮才会出现
  FQ.openBook = async function (title) {
    if (!title) return { ok: false, why: 'no-title' };
    const nodes = [...document.querySelectorAll('div,li,section')]
      .filter((e) => vis(e) && T(e).indexOf(title) >= 0)
      .sort((a, b) => area(a) - area(b));
    if (!nodes.length) return { ok: false, why: 'book-not-found', books: FQ.books() };

    // 从最贴近书名的那层往上爬，找到真正承载按钮的卡片
    let card = null;
    for (const n of nodes.slice(0, 12)) {
      let cur = n;
      for (let i = 0; i < 6 && cur; i++) {
        if (T(cur).indexOf('章节管理') >= 0) {
          card = cur;
          break;
        }
        cur = cur.parentElement;
      }
      if (card) break;
    }
    if (!card) {
      // 按钮是 hover 后才渲染的，先拿书名往上两层的容器当悬停目标
      card = nodes[0];
      for (let i = 0; i < 3 && card.parentElement; i++) {
        if (area(card) > 60000) break;
        card = card.parentElement;
      }
    }
    card.scrollIntoView({ block: 'center' });
    for (const t of ['mouseover', 'mouseenter', 'mousemove']) {
      card.dispatchEvent(new MouseEvent(t, { bubbles: true, clientX: 10, clientY: 10 }));
    }
    await sleep(450);
    const btn = findByText(['章节管理'], { allowHidden: true, maxW: 240, maxH: 70 });
    if (!btn) return { ok: false, why: 'no-manage-button', buttons: buttons() };
    btn.el.click();
    return { ok: true };
  };

  // ---------- 章节管理页：选卷 ----------

  // 卷名形如「第二卷：圣歌与暗金」。注意要排除外层容器——
  // 容器文本会把多个卷名拼在一起，点它并不会真的选中选项。
  function isVol(t) {
    if (!t || t.length > 40) return false;
    if (!/^第[0-9０-９一二三四五六七八九十百零〇]{1,6}卷[\s:：]/.test(t)) return false;
    const n = (t.match(/第[0-9０-９一二三四五六七八九十百零〇]{1,6}卷/g) || []).length;
    return n === 1;
  }

  FQ.volumes = function () {
    const out = [];
    for (const e of pool(false)) {
      const t = T(e);
      if (!isVol(t)) continue;
      if (out.indexOf(t) < 0) out.push(t);
    }
    return out;
  };

  FQ.pickVolume = async function (name) {
    let els = pool(false).filter((e) => isVol(T(e)));
    if (!els.length) return { ok: false, why: 'no-volume', buttons: buttons() };
    // 只有一个：那是下拉触发器，先点开
    if (els.length === 1) {
      els[0].click();
      await sleep(450);
      els = pool(false).filter((e) => isVol(T(e)));
      if (els.length <= 1) return { ok: false, why: 'menu-not-open', options: els.map(T) };
    }
    const target = name ? els.find((e) => T(e).indexOf(name) >= 0) : els[els.length - 1];
    if (!target) return { ok: false, why: 'volume-not-found', options: els.map(T) };
    target.click();
    await sleep(500);
    return { ok: true, picked: T(target), options: els.map(T) };
  };

  FQ.currentVolume = function () {
    const els = pool(false).filter((e) => isVol(T(e)));
    return els.length ? T(els[0]) : '';
  };

  FQ.newChapter = function () {
    return clickText(['新建章节'], { maxW: 320, maxH: 90 });
  };

  FQ.chapterRows = function () {
    // 章节列表里每行标题形如「第25章 25矿道」
    const out = [];
    for (const e of pool(false)) {
      const t = T(e);
      if (!/^第?\d{1,4}\s*章?/.test(t) || t.length > 60) continue;
      out.push(t);
    }
    return out.slice(0, 40);
  };

  // ---------- 新建章：填章号 / 标题 / 正文 ----------

  FQ.fillChapter = function (no, title) {
    const res = { ok: false, no: null, title: null, inputs: inputs() };

    // 章号输入框：占位符含「章」→ 或被「第」「章」夹住的 input → 退化为标题框前面那个
    const all = [...document.querySelectorAll('input')].filter((e) => vis(e));
    let noEl = all.find((e) => /章/.test(e.getAttribute('placeholder') || ''));
    if (!noEl) {
      noEl = all.find((e) => {
        const p = e.parentElement;
        const pt = p ? T(p) + ' ' + T(p.parentElement || p) : '';
        return /第.*章/.test(pt) && !/标题/.test(e.getAttribute('placeholder') || '');
      });
    }
    if (noEl && no != null) {
      setValue(noEl, String(no));
      res.no = noEl.value;
    }

    const titleEl = [...document.querySelectorAll('input,textarea')].find(
      (e) => vis(e) && /标题/.test(e.getAttribute('placeholder') || '')
    );
    if (titleEl && title != null) {
      setValue(titleEl, title);
      res.title = titleEl.value;
    }
    if (noEl) noEl.dispatchEvent(new Event('blur', { bubbles: true }));
    if (titleEl) titleEl.dispatchEvent(new Event('blur', { bubbles: true }));
    res.ok = !!titleEl || !!noEl;
    if (!res.ok) res.buttons = buttons();
    return res;
  };

  FQ.fillBody = async function (text) {
    const el = editorEl();
    if (!el) return { ok: false, why: 'no-editor', inputs: inputs() };

    el.scrollIntoView({ block: 'center' });
    el.focus();
    el.click();
    await sleep(120);

    // 选中全部旧内容再插入，避免残留占位符
    try {
      const sel = window.getSelection();
      const range = document.createRange();
      range.selectNodeContents(el);
      sel.removeAllRanges();
      sel.addRange(range);
      document.execCommand('delete', false, null);
    } catch (e) {}

    let used = 'execCommand';
    try {
      document.execCommand('insertText', false, text);
    } catch (e) {
      used = 'none';
    }

    const got = () => norm(el.innerText || el.value || '');
    // insertText 常把换行吃掉，字数对不上就改用逐段写入
    if (got().length < norm(text).length * 0.9) {
      used = 'paragraphs';
      const esc = (s) =>
        s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
      if (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT') {
        setValue(el, text);
      } else {
        el.innerHTML = text
          .split('\n')
          .filter((l) => l.trim() !== '')
          .map((l) => '<p>' + esc(l) + '</p>')
          .join('');
        el.dispatchEvent(
          new InputEvent('input', { bubbles: true, inputType: 'insertText', data: text })
        );
      }
    }
    await sleep(150);
    return { ok: true, used: used, chars: got().length, want: norm(text).length };
  };

  FQ.bodyState = function () {
    const el = editorEl();
    const titleEl = [...document.querySelectorAll('input,textarea')].find(
      (e) => vis(e) && /标题/.test(e.getAttribute('placeholder') || '')
    );
    return {
      chars: el ? norm(el.innerText || el.value || '').length : 0,
      title: titleEl ? titleEl.value : '',
      url: location.href,
    };
  };

  // ---------- 各种「下一步 / 弹窗」 ----------

  FQ.next = function () {
    return clickText(['下一步'], { skipDisabled: true, maxW: 240, maxH: 80 });
  };

  // 错别字提示弹窗：「检测到你还有错别字未修改，是否确定提交?」
  // 注意：编辑页右侧面板平时就有「暂无错别字」，所以只认「未修改」这种弹窗专用措辞
  FQ.typoDialog = function () {
    const hit = [...document.querySelectorAll('div,section')].find(
      (e) => vis(e) && /错别字未修改|还有错别字|未修改的错别字/.test(T(e)) && T(e).length < 260
    );
    if (!hit) return { shown: false };
    const cancel = findByText(['取消'], { exact: true, maxW: 240, maxH: 80 });
    const submit = findByText(['提交'], { exact: true, maxW: 240, maxH: 80 });
    return { shown: true, hasCancel: !!cancel, hasSubmit: !!submit, kind: 'typo' };
  };

  FQ.cancelTypo = function () {
    return clickText(['取消'], { maxW: 240, maxH: 80 });
  };

  // 智能纠错页：有「替换全部」就点，风险数字也在这里读
  // 注意：编辑页右侧面板也有「智能纠错」标题，所以判据用底部那组操作按钮
  FQ.correctState = function () {
    const body = T(document.body);
    const replaceBtn = findByText(['替换全部'], { maxW: 260, maxH: 80 });
    const ignoreBtn = findByText(['忽略全部'], { maxW: 260, maxH: 80 });
    if (!replaceBtn && !ignoreBtn && body.indexOf('智能纠错') < 0) return { shown: false };
    if (!replaceBtn && !ignoreBtn) return { shown: false };
    let risks = 0;
    const m = body.match(/风险提示\s*[\(（]\s*(\d+)\s*[\)）]/);
    if (m) risks = parseInt(m[1], 10);
    const c = body.match(/智能纠错\s*[\(（]\s*(\d+)\s*[\)）]/);
    return {
      shown: true,
      replaceable: !!replaceBtn && !isDisabled(replaceBtn.el),
      replaceText: replaceBtn ? replaceBtn.text : '',
      corrections: c ? parseInt(c[1], 10) : 0,
      risks: risks,
    };
  };

  FQ.replaceAll = function () {
    const btn = findByText(['替换全部'], { maxW: 260, maxH: 80 });
    if (!btn) return { ok: false, why: 'no-replace-button' };
    if (isDisabled(btn.el)) return { ok: true, skipped: 'disabled' };
    btn.el.click();
    return { ok: true };
  };

  FQ.detectDialog = function () {
    const body = T(document.body);
    if (body.indexOf('内容检测') < 0 || body.indexOf('全面检测') < 0) return { shown: false };
    return {
      shown: true,
      full: !!findByText(['全面检测'], { maxW: 260, maxH: 80 }),
      basic: !!findByText(['仅基础检测'], { maxW: 260, maxH: 80 }),
    };
  };

  FQ.fullDetect = function () {
    return clickText(['全面检测'], { maxW: 260, maxH: 80 });
  };

  // 全面检测结果：出现「风险」「违规」字样且不是「风险提示 (0)」就认为有风险
  FQ.riskState = function () {
    const body = T(document.body);
    const m = body.match(/风险提示\s*[\(（]\s*(\d+)\s*[\)）]/);
    const n = m ? parseInt(m[1], 10) : 0;
    const warn =
      /检测到(风险|敏感|违规)/.test(body) ||
      /内容存在风险/.test(body) ||
      /疑似(违规|风险)/.test(body);
    const publish = body.indexOf('发布设置') >= 0;
    return { risks: n, warn: warn, publishShown: publish };
  };

  // ---------- 发布设置 ----------

  FQ.publishState = function () {
    const body = T(document.body);
    const shown = body.indexOf('发布设置') >= 0;
    if (!shown) return { shown: false, buttons: buttons(30) };
    const ins = [...document.querySelectorAll('input')].filter((e) => vis(e));
    return {
      shown: true,
      confirm: !!findByText(['确认发布'], { maxW: 260, maxH: 80 }),
      inputs: ins.map((e) => ({
        ph: e.getAttribute('placeholder') || '',
        type: e.getAttribute('type') || '',
        val: e.value || '',
      })),
    };
  };

  FQ.setPublish = async function (dateStr, timeStr) {
    const res = { ok: true, steps: [] };
    const body = () => T(document.body);

    // 1) 是否使用AI → 选「否」
    // 「否」必须精确匹配，否则会命中「是否使用AI」这一行本身
    let aiDone = false;
    const aiLabel = findByText(['是否使用AI'], { maxW: 320, maxH: 60 });
    if (aiLabel) {
      let row = aiLabel.el;
      for (let i = 0; i < 3 && row; i++) {
        if (row.querySelector('input[type="radio"]')) break;
        row = row.parentElement;
      }
      const radios = row ? [...row.querySelectorAll('input[type="radio"]')] : [];
      if (radios.length >= 2) {
        radios[1].click();            // 第二个就是「否」
        aiDone = true;
      }
    }
    if (!aiDone) {
      const noEl = findByText(['否'], { exact: true, maxW: 90, maxH: 60 });
      if (noEl) {
        const r = noEl.el.querySelector && noEl.el.querySelector('input[type="radio"]');
        (r || noEl.el).click();
        aiDone = true;
      }
    }
    res.steps.push(aiDone ? 'AI=否' : 'AI=否(未找到)');
    await sleep(250);

    // 2) 定时发布开关
    const swLabel = findByText(['定时发布'], { maxW: 260, maxH: 60 });
    let sw = null;
    if (swLabel) {
      let p = swLabel.el;
      for (let i = 0; i < 4 && p; i++) {
        const cand = p.querySelector && p.querySelector('[role="switch"],.ant-switch,input[type="checkbox"]');
        if (cand) {
          sw = cand;
          break;
        }
        const next = p.nextElementSibling;
        if (next && next.querySelector) {
          const cand2 = next.matches('[role="switch"],.ant-switch,input[type="checkbox"]')
            ? next
            : next.querySelector('[role="switch"],.ant-switch,input[type="checkbox"]');
          if (cand2) {
            sw = cand2;
            break;
          }
        }
        p = p.parentElement;
      }
    }
    if (!sw) {
      // 退化：整块找 role=switch
      sw = document.querySelector('[role="switch"],.ant-switch');
    }
    if (sw) {
      const on =
        sw.getAttribute('aria-checked') === 'true' ||
        /checked|ant-switch-checked/.test((sw.className || '') + '') ||
        (sw.tagName === 'INPUT' && sw.checked);
      if (!on) sw.click();
      res.steps.push(on ? '定时发布(已开)' : '定时发布=开');
    } else {
      res.steps.push('定时发布(未找到)');
    }
    await sleep(400);

    // 3) 日期 / 时间：按 placeholder、type、再按出现顺序兜底
    const ins = [...document.querySelectorAll('input')].filter((e) => vis(e));
    const dateEl =
      ins.find((e) => /日期/.test(e.getAttribute('placeholder') || '')) ||
      ins.find((e) => e.getAttribute('type') === 'date') ||
      ins.find((e) => /^\d{4}-\d{2}-\d{2}$/.test(e.value || ''));
    const timeEl =
      ins.find((e) => /时间/.test(e.getAttribute('placeholder') || '')) ||
      ins.find((e) => e.getAttribute('type') === 'time') ||
      ins.find((e) => /^\d{2}:\d{2}/.test(e.value || ''));

    if (dateEl) {
      setValue(dateEl, dateStr);
      res.steps.push('日期=' + dateEl.value);
    } else res.steps.push('日期(未找到)');
    if (timeEl) {
      setValue(timeEl, timeStr);
      res.steps.push('时间=' + timeEl.value);
    } else res.steps.push('时间(未找到)');

    await sleep(250);
    // 4) 回读校验（自定义日期控件可能不接受直接赋值）
    res.dateVal = dateEl ? dateEl.value : '';
    res.timeVal = timeEl ? timeEl.value : '';
    res.ok = res.dateVal === dateStr && res.timeVal.indexOf(timeStr) === 0;
    if (!res.ok) res.buttons = buttons(30);
    return res;
  };

  FQ.confirmPublish = function () {
    return clickText(['确认发布'], { maxW: 260, maxH: 80, skipDisabled: true });
  };

  // 发布成功后的判据：回到章节管理页 / 出现成功提示
  FQ.published = function () {
    const body = T(document.body);
    return {
      back: body.indexOf('新建章节') >= 0 && body.indexOf('发布设置') < 0,
      success: /发布成功|提交成功|已提交/.test(body),
      url: location.href,
    };
  };

  // 异常兜底：把当前章存成草稿，避免白写
  // 发布设置/检测弹窗会挡住「存草稿」，先把它们取消掉
  FQ.saveDraft = async function () {
    for (let i = 0; i < 3; i++) {
      const c = findByText(['取消'], { exact: true, maxW: 240, maxH: 80 });
      if (!c) break;
      c.el.click();
      await sleep(400);
    }
    let btn = findByText(['存草稿'], { maxW: 240, maxH: 80 });
    if (!btn) {
      await sleep(600);
      btn = findByText(['存草稿'], { maxW: 240, maxH: 80 });
    }
    if (!btn) return { ok: false, why: 'no-save-button', buttons: buttons(30) };
    btn.el.click();
    await sleep(900);
    // 可能再弹一个「确定保存草稿？」之类
    const sure = findByText(['确定'], { exact: true, maxW: 200, maxH: 70 });
    if (sure) {
      sure.el.click();
      await sleep(500);
    }
    return { ok: true };
  };

  FQ.navigate = function (url) {
    location.href = url;
    return true;
  };

  /* 「下一步」之后这条链的决策器：
   *   错别字弹窗 → 取消 → 智能纠错页 → 替换全部 → 下一步 → (可能再弹) → 全面检测 → 发布设置
   * 每次调用只做一个动作，由 C++ 侧按返回的 act 决定下一步并控制节奏。
   */
  FQ.autoStep = async function () {
    const typo = FQ.typoDialog();
    if (typo.shown) {
      const r = FQ.cancelTypo();
      return { act: r.ok ? 'cancelTypo' : 'wait', hasCancel: typo.hasCancel };
    }

    const det = FQ.detectDialog();
    if (det.shown) {
      const r = FQ.fullDetect();
      return { act: r.ok ? 'fullDetect' : 'wait', buttons: r.ok ? null : r.buttons };
    }

    const cor = FQ.correctState();
    if (cor.shown) {
      if (cor.risks > 0)
        return { act: 'risk', risks: cor.risks, corrections: cor.corrections };
      if (cor.replaceable) {
        FQ.replaceAll();
        await sleep(650);
        const r = FQ.next();
        return { act: 'replaceNext', next: !!r.ok };
      }
      const r = FQ.next();
      return { act: 'next', next: !!r.ok, corrections: cor.corrections };
    }

    if (FQ.publishState().shown) return { act: 'publish' };

    return { act: 'wait', stage: FQ.stage() };
  };

  window.__fq = FQ;
  return 'ok';
})();
