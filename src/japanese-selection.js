/* Runs inside Nickel's current KEPUB WebKit frame. ES5 only, no network/storage. */
function dtpJapaneseSelection() {
    function reply(status, word) { return JSON.stringify({status: status, word: word || ""}); }
    function japanese(text) { return /[\u3040-\u30ff\u3400-\u9fff\uff66-\uff9f]/.test(text); }
    function excluded(node) {
        return node && node.nodeType === 1 && /^(RT|RP|SCRIPT|STYLE|NOSCRIPT|HEAD)$/.test(node.tagName.toUpperCase());
    }
    function block(node) {
        return node && node.nodeType === 1 && /^(P|DIV|LI|DT|DD|H[1-6]|TD|TH|BLOCKQUOTE|SECTION|ARTICLE|BODY|PRE)$/.test(node.tagName.toUpperCase());
    }
    function plain(fragment) {
        var result = "", count = 0;
        function visit(node) {
            if (++count > 4096) throw new Error("limit");
            if (excluded(node)) return;
            if (node.nodeType === 3) result += node.nodeValue;
            else for (var c = node.firstChild; c; c = c.nextSibling) visit(c);
        }
        visit(fragment);
        return result;
    }
    var selection = window.getSelection();
    if (!selection || selection.rangeCount !== 1 || selection.isCollapsed) return reply("reject");
    var original = selection.getRangeAt(0).cloneRange();
    var changed = false;
    try {
        var initial = plain(original.cloneContents());
        if (!japanese(initial)) return reply("native");
        var anchor = original.startContainer;
        for (var ancestor = anchor.nodeType === 3 ? anchor.parentNode : anchor; ancestor; ancestor = ancestor.parentNode)
            if (excluded(ancestor)) return reply("reject");
        var root = anchor.nodeType === 3 ? anchor.parentNode : anchor;
        while (root && !block(root)) root = root.parentNode;
        if (!root || !root.contains(original.endContainer)) return reply("reject");

        var entries = [], text = "", visited = 0;
        function collect(node) {
            if (++visited > 4096 || entries.length > 512 || text.length > 4096) throw new Error("limit");
            if (excluded(node)) return;
            if (node.nodeType === 3) {
                entries.push({node: node, start: text.length, length: node.nodeValue.length});
                text += node.nodeValue;
                return;
            }
            if (node.nodeType !== 1 && node !== root) return;
            if (node.nodeType === 1 && node !== root) {
                var style = window.getComputedStyle(node, null);
                if (style && (style.display === "none" || style.visibility === "hidden")) return;
                if (node.tagName.toUpperCase() === "BR") { text += "\n"; return; }
            }
            var separate = node !== root && block(node);
            if (separate) text += "\n";
            for (var child = node.firstChild; child; child = child.nextSibling) collect(child);
            if (separate) text += "\n";
        }
        collect(root);
        if (!entries.length || text.length > 4096) return reply("reject");

        function absolute(container, offset) {
            for (var i = 0; i < entries.length; ++i) {
                var e = entries[i];
                if (e.node === container) {
                    if (offset < 0 || offset > e.length) throw new Error("offset");
                    return e.start + offset;
                }
            }
            var point = document.createRange();
            point.setStart(container, offset); point.collapse(true);
            for (var j = 0; j < entries.length; ++j) {
                var probe = document.createRange();
                probe.setStart(entries[j].node, 0); probe.collapse(true);
                if (point.compareBoundaryPoints(0, probe) <= 0) return entries[j].start;
            }
            return text.length;
        }
        var start = absolute(original.startContainer, original.startOffset);
        var end = absolute(original.endContainer, original.endOffset);
        if (start >= end || text.slice(start, end) !== initial) return reply("reject");

        var request = typeof dtpRequest === "undefined" ? null : dtpRequest;
        if (request && request.mode === "context")
            return JSON.stringify({status:"context",text:text,start:start,end:end,initial:initial});
        var chosen = null;
        if (request && request.mode === "apply") {
            if (request.text !== text || request.initialStart !== start || request.initialEnd !== end ||
                request.start !== Math.floor(request.start) || request.end !== Math.floor(request.end) ||
                request.start < 0 || request.start > start || request.end < end || request.end > text.length ||
                text.slice(request.start,request.end) !== request.word) return reply("reject");
            chosen = {start:request.start,end:request.end,word:request.word};
        } else {
        // Default release backend. Optional Sudachi apply mode supplies bounds above.
        var tokens = new TinySegmenter().segment(text);
        if (tokens.join("") !== text) return reply("reject");
        var at = 0;
        // Force punctuation/space boundaries even if the statistical model joins them.
        var delimiter = /([^A-Za-z0-9\u00c0-\u024f\u0300-\u036f\u3005\u3006\u3041-\u3096\u3099-\u309f\u30a1-\u30fa\u30fc-\u30ff\u3400-\u9fff\uff10-\uff19\uff21-\uff3a\uff41-\uff5a\uff66-\uff9f]+)/;
        for (var t = 0; t < tokens.length; ++t) {
            // Keep surrogate pairs and combining kana intact across model boundaries.
            while (t + 1 < tokens.length &&
                   (/[\uD800-\uDBFF]$/.test(tokens[t]) && /^[\uDC00-\uDFFF]/.test(tokens[t+1]) ||
                    /^[\u3099\u309a\u0300-\u036f\ufe00-\ufe0f]/.test(tokens[t+1])))
                tokens[t] += tokens.splice(t+1, 1)[0];
            var pieces = tokens[t].split(delimiter);
            for (var p = 0; p < pieces.length; ++p) {
                var piece = pieces[p], next = at + piece.length;
                if (piece && at <= start && next >= end && !delimiter.test(piece))
                    chosen = {start: at, end: next, word: piece};
                at = next;
            }
        }
        }
        if (!chosen || !japanese(chosen.word) || chosen.word.length > 24 ||
            /[^A-Za-z0-9'\-\u2019\u00c0-\u024f\u0300-\u036f\u3005\u3006\u3041-\u3096\u3099-\u309f\u30a1-\u30fa\u30fc-\u30ff\u3400-\u9fff\uff10-\uff19\uff21-\uff3a\uff41-\uff5a\uff66-\uff9f]/.test(chosen.word)) return reply("reject");

        function boundary(position, isEnd) {
            for (var i = 0; i < entries.length; ++i) {
                var e = entries[i], finish = e.start + e.length;
                if (e.length && position >= e.start && (position < finish || isEnd && position === finish))
                    return {node: e.node, offset: position - e.start};
            }
            throw new Error("boundary");
        }
        var left = boundary(chosen.start, false), right = boundary(chosen.end, true);
        var expanded = document.createRange();
        expanded.setStart(left.node, left.offset); expanded.setEnd(right.node, right.offset);
        if (plain(expanded.cloneContents()) !== chosen.word) return reply("reject");
        selection.removeAllRanges(); selection.addRange(expanded); changed = true;
        if (selection.rangeCount !== 1 || plain(selection.getRangeAt(0).cloneContents()) !== chosen.word)
            throw new Error("selection");
        return reply("expanded", chosen.word);
    } catch (error) {
        if (changed) { selection.removeAllRanges(); selection.addRange(original); }
        return reply("reject");
    }
}
return dtpJapaneseSelection();
