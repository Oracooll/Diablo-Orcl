const fs = require('fs');
const src = fs.readFileSync(process.argv[2], 'utf8');
const str = '"((?:[^"\\x5c]|\\x5c.)*)"';
const re = new RegExp('\\{\\s*N_\\(' + str + '\\),\\s*N_\\(' + str + '\\),\\s*(Pal|Bar|Sor|Rog|Bard|Monk|Nec),\\s*([A-Za-z0-9]+),\\s*(\\d+),\\s*(\\d+),\\s*Kind::(\\w+),\\s*SpellID::(\\w+),\\s*(true|false)(?:,\\s*(\\d+))?\\s*\\}', 'g');
const rows = [];
let m;
while ((m = re.exec(src))) {
	const line = src.slice(0, m.index).split('\n').length;
	rows.push({ line, name: m[1], desc: m[2], cls: m[3], page: m[4], tier: +m[5], col: +m[6], kind: m[7], spell: m[8], impl: m[9] === 'true' });
}
const by = {};
for (const r of rows) {
	by[r.cls] = by[r.cls] || { total: 0, inert: 0 };
	by[r.cls].total++;
	if (!r.impl) by[r.cls].inert++;
}
console.log(rows.length, JSON.stringify(by));
for (const r of rows.filter(r => !r.impl))
	console.log([r.line, r.cls, r.page, r.tier + '.' + r.col, r.kind, r.spell, r.name, '::', r.desc].join(' | '));
fs.writeFileSync(process.argv[3], JSON.stringify(rows, null, 1));
