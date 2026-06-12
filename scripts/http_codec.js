// HTTP (URL & HTML) Encoding/Decoding functions for Ecode
console.log("Loading HTTP Codec plugin...");

// Ensure the mx command registry exists
if (typeof emacs_mx_commands === 'undefined') {
    var emacs_mx_commands = {};
}

// ---------------------------------------------------------------------------
// URL Encoding / Decoding
// ---------------------------------------------------------------------------

function url_encode_interactive() {
    var pos = Editor.getCaretPos();
    var anchor = Editor.getSelectionAnchor();
    if (pos !== anchor) {
        var start = Math.min(pos, anchor);
        var end = Math.max(pos, anchor);
        var text = Editor.getText(start, end - start);
        var encoded = encodeURIComponent(text);
        Editor.delete(start, end - start);
        Editor.insert(start, encoded);
        Editor.setSelectionAnchor(start);
        Editor.setCaretPos(start + encoded.length);
        Editor.setStatusText("URL Encoded selection");
    } else {
        Editor.showMinibuffer("URL Encode: ", "callback", "on_url_encode_input");
    }
}

function on_url_encode_input(input) {
    if (!input) return;
    var encoded = encodeURIComponent(input);
    var pos = Editor.getCaretPos();
    Editor.insert(pos, encoded);
    Editor.setCaretPos(pos + encoded.length);
    Editor.setStatusText("Inserted encoded URL");
}

function url_decode_interactive() {
    var pos = Editor.getCaretPos();
    var anchor = Editor.getSelectionAnchor();
    if (pos !== anchor) {
        var start = Math.min(pos, anchor);
        var end = Math.max(pos, anchor);
        var text = Editor.getText(start, end - start);
        try {
            var decoded = decodeURIComponent(text);
            Editor.delete(start, end - start);
            Editor.insert(start, decoded);
            Editor.setSelectionAnchor(start);
            Editor.setCaretPos(start + decoded.length);
            Editor.setStatusText("URL Decoded selection");
        } catch (e) {
            Editor.setStatusText("Error decoding URL: " + e.message);
        }
    } else {
        Editor.showMinibuffer("URL Decode: ", "callback", "on_url_decode_input");
    }
}

function on_url_decode_input(input) {
    if (!input) return;
    try {
        var decoded = decodeURIComponent(input);
        var pos = Editor.getCaretPos();
        Editor.insert(pos, decoded);
        Editor.setCaretPos(pos + decoded.length);
        Editor.setStatusText("Inserted decoded URL");
    } catch (e) {
        Editor.setStatusText("Error decoding URL: " + e.message);
    }
}

// ---------------------------------------------------------------------------
// HTML Encoding / Decoding
// ---------------------------------------------------------------------------

function html_encode_string(str) {
    return str.replace(/&/g, '&amp;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#39;');
}

function html_decode_string(str) {
    // Basic HTML entity decoding
    return str.replace(/&lt;/g, '<')
              .replace(/&gt;/g, '>')
              .replace(/&quot;/g, '"')
              .replace(/&#39;/g, "'")
              .replace(/&amp;/g, '&');
}

function html_encode_interactive() {
    var pos = Editor.getCaretPos();
    var anchor = Editor.getSelectionAnchor();
    if (pos !== anchor) {
        var start = Math.min(pos, anchor);
        var end = Math.max(pos, anchor);
        var text = Editor.getText(start, end - start);
        var encoded = html_encode_string(text);
        Editor.delete(start, end - start);
        Editor.insert(start, encoded);
        Editor.setSelectionAnchor(start);
        Editor.setCaretPos(start + encoded.length);
        Editor.setStatusText("HTML Encoded selection");
    } else {
        Editor.showMinibuffer("HTML Encode: ", "callback", "on_html_encode_input");
    }
}

function on_html_encode_input(input) {
    if (!input) return;
    var encoded = html_encode_string(input);
    var pos = Editor.getCaretPos();
    Editor.insert(pos, encoded);
    Editor.setCaretPos(pos + encoded.length);
    Editor.setStatusText("Inserted encoded HTML");
}

function html_decode_interactive() {
    var pos = Editor.getCaretPos();
    var anchor = Editor.getSelectionAnchor();
    if (pos !== anchor) {
        var start = Math.min(pos, anchor);
        var end = Math.max(pos, anchor);
        var text = Editor.getText(start, end - start);
        var decoded = html_decode_string(text);
        Editor.delete(start, end - start);
        Editor.insert(start, decoded);
        Editor.setSelectionAnchor(start);
        Editor.setCaretPos(start + decoded.length);
        Editor.setStatusText("HTML Decoded selection");
    } else {
        Editor.showMinibuffer("HTML Decode: ", "callback", "on_html_decode_input");
    }
}

function on_html_decode_input(input) {
    if (!input) return;
    var decoded = html_decode_string(input);
    var pos = Editor.getCaretPos();
    Editor.insert(pos, decoded);
    Editor.setCaretPos(pos + decoded.length);
    Editor.setStatusText("Inserted decoded HTML");
}

// Register commands in M-x menu
emacs_mx_commands["url-encode"] = url_encode_interactive;
emacs_mx_commands["url-decode"] = url_decode_interactive;
emacs_mx_commands["html-encode"] = html_encode_interactive;
emacs_mx_commands["html-decode"] = html_decode_interactive;
