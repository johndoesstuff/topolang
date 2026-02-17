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
	for (let i = 0; i < set.length; i++) {
		div.appendChild(generateNodes(set[i], depth + 1));
	}
	return div;
}

function generate() {
	document.getElementById("container").appendChild(generateNodes(parseSet(document.getElementById("in").value), 1))
}
