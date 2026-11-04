const assert = require('node:assert/strict');
const fs = require('node:fs');
const {chromium} = require('playwright');

async function main() {
  const script = fs.readFileSync('build/japanese-bundle.js', 'utf8');
  const browser = await chromium.launch({headless:true, executablePath:process.env.SIMULATOR_CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe'});
  let count = 0;
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    async function check({html,selector='#tap',start=0,end=start+1,word,status='expanded',element=false,payload=script}) {
      await page.setContent(html);
      const before = await page.evaluate(({selector,start,end,element}) => {
        const target = document.querySelector(selector);
        const node = element ? target : target.firstChild;
        const range = document.createRange(); range.setStart(node,start); range.setEnd(node,end);
        const selection = window.getSelection(); selection.removeAllRanges(); selection.addRange(range);
        return selection.toString();
      }, {selector,start,end,element});
      const result = JSON.parse(await page.evaluate(payload));
      assert.equal(result.status,status);
      if (word !== undefined) assert.equal(result.word,word);
      const selected = await page.evaluate(() => {
        const range = window.getSelection().getRangeAt(0), fragment = range.cloneContents();
        for (const node of fragment.querySelectorAll('rt,rp,script,style')) node.remove();
        return {text:fragment.textContent, raw:window.getSelection().toString(), start:range.startOffset,end:range.endOffset};
      });
      if (status==='expanded') assert.equal(selected.text,word);
      else assert.equal(selected.raw,before,'rejection/native fallback must preserve original range');
      ++count;
      return selected;
    }
    await check({html:'<p id="tap">今日は高校に通います。</p>',start:3,word:'高校'});
    await check({html:'<p id="tap">今日は高校に通います。</p>',start:4,word:'高校'});
    // Optional private device example remains in ignored build/, never a source fixture.
    if (fs.existsSync('build/japanese-marked-span.txt')) {
      const text = fs.readFileSync('build/japanese-marked-span.txt','utf8');
      const escaped = text.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
      await check({html:'<p id="tap">'+escaped+'</p>',start:6,word:'高校'});
    }
    await check({html:'<p>今日は<span id="tap">学</span><em>校</em>に行きます。</p>',word:'学校'});
    await check({html:'<p>今日は<ruby><span id="tap">学</span><rt>がっ</rt></ruby><ruby>校<rt>こう</rt></ruby>に行きます。</p>',word:'学校'});
    await check({html:'<p>今日は<ruby>学校<rt id="tap">がっこう</rt></ruby>に行きます。</p>',status:'reject'});
    const repeated = await check({html:'<p>高校と<span id="tap">高校</span>は同じ表記。</p>',word:'高校'});
    assert.equal(repeated.start,0);
    await check({html:'<p>😀<span id="tap">高校</span>に通う。</p>',word:'高校'});
    await check({html:'<p>私は<span id="tap">日本語</span>を勉強しています。</p>',word:'日本語'});
    await check({html:'<p id="tap">本を食べる猫。</p>',start:2,word:'食べる'});
    await check({html:'<p><span id="tap">「</span>高校」</p>',status:'native'});
    await check({html:'<p id="tap">plain English text</p>',start:6,end:13,status:'native'});
    await check({html:'<p id="tap">高校</p><p>時代</p>',word:'高校'});
    await check({html:'<p id="tap"><span>高</span><span>校</span>時代</p>',element:true,start:0,end:1,word:'高校'});
    await check({html:'<p>高<span style="display:none">hidden</span><span id="tap">校</span>時代</p>',status:'reject'});
    await check({html:'<p id="tap">'+'あ'.repeat(5000)+'高校</p>',start:5000,status:'reject'});
    await check({html:'<p id="tap">今日は高校に通います。</p>',start:0,end:11,status:'reject'});
    // Even an under-segmenting model cannot authorize an arbitrarily long token.
    const coarseModel = script.replace('new TinySegmenter().segment(text)', '[text]');
    assert.notEqual(coarseModel,script);
    await check({html:'<p id="tap">'+'高'.repeat(25)+'</p>',status:'reject',payload:coarseModel});
    // Cross-block native selections are never broadened to neighboring paragraphs.
    await page.setContent('<p id="a">高</p><p id="b">校</p>');
    await page.evaluate(() => { const r=document.createRange();r.setStart(document.querySelector('#a').firstChild,0);r.setEnd(document.querySelector('#b').firstChild,1);window.getSelection().removeAllRanges();window.getSelection().addRange(r); });
    assert.equal(JSON.parse(await page.evaluate(script)).status,'reject'); ++count;
    assert.deepEqual(errors,[]);
    console.log(`PASS: ${count} Japanese token/DOM-range cases, cross-span/ruby mapping, repeated occurrences, UTF-16 offsets, protected markup, bounded context, native fallback`);
  } finally { await browser.close(); }
}
main().catch(e => { console.error(e); process.exitCode=1; });
