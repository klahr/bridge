.pragma library

// Red suit symbols in a text, for showing calls and contracts in rich text.
function colored(text, red) {
    return String(text).replace(/([♥♦])/g, "<font color=\"" + red + "\">$1</font>")
}

// The strain of a bid's call id, -1 for pass, double and redouble.
function strainOf(callId) { return callId >= 3 ? (callId - 3) % 5 : -1 }
function levelOf(callId) { return callId >= 3 ? Math.floor((callId - 3) / 5) + 1 : 0 }
function bidId(level, strain) { return 3 + (level - 1) * 5 + strain }

// Columns of the auction: west, north, east, south.
function column(seat) { return (seat + 3) % 4 }
