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

function generateNodes(set, depth) {
	let div = document.createElement("span");
	div.innerText = "";
	div.classList.add((depth % 2 == 0) ? "a" : "b");
	div.classList.add("box");
	if (depth == 1) div.style.margin = "auto";
	for (let i = 0; i < set.length; i++) {
		div.appendChild(generateNodes(set[i], depth + 1));
	}
	return div;
}

function generate() {
	let container = document.getElementById("container");
	let inp = document.getElementById("in").value;
	let dbj = Module.ulc2dbj(inp);
	let slc = Module.ulc2slc(inp);
	document.getElementById("dbj").innerText = dbj;
	document.getElementById("slc").innerText = slc;
	container.innerHTML = "";
	container.appendChild(generateNodes(parseSet(slc), 1));
}
