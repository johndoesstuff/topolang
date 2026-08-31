const objectDepth = (o) => Object (o) === o ? 1 + Math.max(-1, ...Object.values(o).map(objectDepth)) : 0

function parseSet(str) {
    const stack = [[]];
    let i = 0;
    while (i < str.length) {
        if (str[i] === '{') {
            const next = [];
            stack[stack.length - 1].push(next);
            stack.push(next);
        } else if (str[i] === '}') {
            stack.pop();
        }
        i++;
    }
    return stack[0][0];
}

function makeNode(depth) {
	let div = document.createElement("span");
	div.classList.add((depth % 2 == 0) ? "a" : "b");
	div.classList.add("box");
	if (depth == 1) div.style.margin = "auto";
	return div;
}

// canonical structural signature of a set (children sorted since sets are
// unordered), cached on the array
function sig(set) {
	if (set._sig === undefined)
		set._sig = "{" + set.map(sig).sort().join(",") + "}";
	return set._sig;
}

const ANIM_MS = 250;

// update the span `el` to display `set`, reusing existing child spans where
// possible so that only the parts that changed animate:
//   1. children with an identical structure are kept as-is
//   2. remaining children are paired with the closest-sized leftover and
//      reconciled recursively
//   3. anything left over enters/leaves with a css transition
function reconcile(el, set, depth) {
	el.dataset.sig = sig(set);
	const old = Array.from(el.children).filter((c) => !c.classList.contains("leave"));
	const used = new Set();
	const pending = [];

	for (const child of set) {
		const i = old.findIndex((c, i) => !used.has(i) && c.dataset.sig === sig(child));
		if (i >= 0) used.add(i);
		else pending.push(child);
	}

	for (const child of pending) {
		let best = -1, bestDist = Infinity;
		old.forEach((c, i) => {
			if (used.has(i)) return;
			const dist = Math.abs(c.childElementCount - child.length);
			if (dist < bestDist) { bestDist = dist; best = i; }
		});
		if (best >= 0) {
			used.add(best);
			reconcile(old[best], child, depth + 1);
		} else {
			const node = makeNode(depth + 1);
			node.classList.add("enter");
			el.appendChild(node);
			reconcile(node, child, depth + 1);
			entering.push(node);
		}
	}

	old.forEach((c, i) => {
		if (used.has(i)) return;
		leaving.push(c);
	});
}

// auto sizes can't transition, so pin a node's size in px for the duration
// of an animation and release it afterwards
function freeze(node) {
	node.style.width = node.offsetWidth + "px";
	node.style.height = node.offsetHeight + "px";
}
function thaw(node) {
	node.style.width = "";
	node.style.height = "";
}

// scale the container so a topology of the given natural size fits the viewport
let lastSize = [0, 0];
function fit(w, h) {
	lastSize = [w, h];
	const container = document.getElementById("container");
	const availW = container.clientWidth;
	const availH = window.innerHeight - container.offsetTop - 20;
	const s = Math.min(1, availW / w, availH / h);
	container.style.transform = "scale(" + s + ")";
	// the layout box doesn't shrink with the transform; keep the page from
	// scrolling by reserving only the scaled height
	container.style.height = (h * s) + "px";
}
function refit() { if (lastSize[0]) fit(...lastSize); }
window.addEventListener("resize", refit);
// the display sits below #dbj, so its available height changes whenever the
// De Bruijn field expands, collapses, or rewraps
new ResizeObserver(refit).observe(document.getElementById("dbj"));

// nodes created / orphaned during the current render, animated afterwards
let entering = [];
let leaving = [];

function render(slc) {
	let container = document.getElementById("container");
	let root = container.firstElementChild;
	if (!root) {
		root = makeNode(1);
		container.appendChild(root);
	}
	entering = [];
	leaving = [];
	reconcile(root, parseSet(slc), 1);
	const added = entering, removed = leaving;

	// measure the final layout: new nodes at full size, leaving nodes gone.
	// nothing paints until this function returns, so the toggling is invisible
	added.forEach((n) => n.classList.remove("enter"));
	removed.forEach((n) => n.style.display = "none");
	const sizes = added.map((n) => [n.offsetWidth, n.offsetHeight]);
	fit(root.offsetWidth, root.offsetHeight);
	added.forEach((n) => n.classList.add("enter"));
	removed.forEach((n) => n.style.display = "");

	// pin leaving nodes at their current size so the collapse has a start value
	removed.forEach(freeze);
	void container.offsetHeight;
	removed.forEach((n) => {
		n.classList.add("leave");
		setTimeout(() => n.remove(), ANIM_MS);
	});

	// let the collapsed state paint once, then grow into the measured size
	if (!added.length) return;
	requestAnimationFrame(() => requestAnimationFrame(() => {
		added.forEach((n, i) => {
			n.style.width = sizes[i][0] + "px";
			n.style.height = sizes[i][1] + "px";
			n.classList.remove("enter");
		});
		// release to auto only once everything around has settled
		setTimeout(() => added.forEach(thaw), ANIM_MS + 50);
	}));
}

function generate() {
	let inp = document.getElementById("in").value;
	let dbj = Module.ulc2dbj(inp);
	let slc = Module.ulc2slc(inp);
	document.getElementById("dbj").innerText = dbj;
	document.getElementById("slc").value = slc;
	render(slc);
}

let Reducer = null;
ReducerModule().then((m) => { Reducer = m; });

function display() {
	let slc = document.getElementById("slc").value;
	if (Reducer)
		document.getElementById("dbj").innerText = Reducer.slc2dbj(slc);
	render(slc);
}

function reduce() {
	if (!Reducer) return;
	let slc = document.getElementById("slc").value;
	let next = Reducer.reduce_slc(slc);
	document.getElementById("slc").value = next;
	document.getElementById("dbj").innerText = Reducer.slc2dbj(next);
	render(next);
}
