/* Shared behaviour for every wiki page: navigation state, and the sortable/filterable table. */

/* ---------- navigation ---------- */
const NAV = [
	{ heading: 'Start here' },
	{ href: 'index.html', label: 'Overview' },
	{ href: 'mechanics.html', label: 'Core mechanics' },
	{ heading: 'Items' },
	{ href: 'items.html', label: 'Base items' },
	{ href: 'tiers.html', label: 'Base tiers' },
	{ href: 'affixes.html', label: 'Quality and affixes' },
	{ heading: 'Abilities' },
	{ href: 'spells.html', label: 'Spells' },
	{ href: 'skills.html', label: 'Class trees' },
	{ heading: 'World' },
	{ href: 'areas.html', label: 'Areas and levels' },
	{ href: 'monsters.html', label: 'Monsters' },
	{ heading: 'Interface' },
	{ href: 'ui.html', label: 'HUD and windows' },
	{ href: 'controls.html', label: 'Controls' },
	{ href: 'options.html', label: 'INI options' },
	{ heading: 'Project' },
	{ href: 'assets.html', label: 'Art assets' },
	{ href: 'history.html', label: 'Version history' },
];

function buildNav() {
	const here = location.pathname.split('/').pop() || 'index.html';
	let html = '<div class="brand"><b>Diablo Orcl V1</b><span>v' + WIKI.version + ' &middot; ' + WIKI.generated + '</span></div>';
	for (const entry of NAV) {
		if (entry.heading) {
			html += '<h4>' + entry.heading + '</h4>';
		} else {
			const active = entry.href === here ? ' class="active"' : '';
			html += '<a href="' + entry.href + '"' + active + '>' + entry.label + '</a>';
		}
	}
	const nav = document.querySelector('nav.side');
	if (nav) nav.innerHTML = html;

	const foot = document.querySelector('footer');
	if (foot) {
		foot.innerHTML = 'Generated from Source/ by <code>tools/BuildWiki.ps1</code> &middot; v' + WIKI.version
			+ ' &middot; ' + WIKI.generated + ' &middot; re-run the script after any data change.';
	}
}

/* ---------- tables ----------
 *
 * One renderer for every table on the site. Columns are declared per page as
 * { key, label, num?, cls?, render? }; the data is an array of plain objects.
 */
function makeTable(mount, rows, columns, opts) {
	opts = opts || {};
	const state = { sort: opts.sort || null, dir: opts.dir || 1, query: '', filters: {} };

	const tools = document.createElement('div');
	tools.className = 'tools';

	const search = document.createElement('input');
	search.type = 'search';
	search.placeholder = opts.placeholder || 'Search...';
	search.addEventListener('input', () => { state.query = search.value.toLowerCase(); draw(); });
	tools.appendChild(search);

	(opts.filters || []).forEach(f => {
		const sel = document.createElement('select');
		const values = Array.from(new Set(rows.map(r => r[f.key]))).filter(v => v !== '' && v !== undefined).sort();
		sel.innerHTML = '<option value="">' + f.label + ': all</option>'
			+ values.map(v => '<option value="' + v + '">' + v + '</option>').join('');
		sel.addEventListener('change', () => { state.filters[f.key] = sel.value; draw(); });
		tools.appendChild(sel);
	});

	const count = document.createElement('span');
	count.className = 'count';
	tools.appendChild(count);

	const wrap = document.createElement('div');
	wrap.className = 'tablewrap';
	const table = document.createElement('table');
	wrap.appendChild(table);

	mount.appendChild(tools);
	mount.appendChild(wrap);

	function visible() {
		return rows.filter(r => {
			for (const key in state.filters) {
				if (state.filters[key] && String(r[key]) !== state.filters[key]) return false;
			}
			if (!state.query) return true;
			return columns.some(c => String(r[c.key] === undefined ? '' : r[c.key]).toLowerCase().includes(state.query));
		});
	}

	function draw() {
		let data = visible();
		if (state.sort) {
			const key = state.sort;
			data = data.slice().sort((a, b) => {
				const x = a[key], y = b[key];
				if (typeof x === 'number' && typeof y === 'number') return (x - y) * state.dir;
				return String(x).localeCompare(String(y)) * state.dir;
			});
		}

		let head = '<thead><tr>';
		for (const c of columns) {
			const arrow = state.sort === c.key ? '<span class="arrow">' + (state.dir > 0 ? '▲' : '▼') + '</span>' : '';
			head += '<th class="' + (c.num ? 'num' : '') + '" data-key="' + c.key + '">' + c.label + arrow + '</th>';
		}
		head += '</tr></thead>';

		let body = '<tbody>';
		for (const r of data) {
			body += '<tr>';
			for (const c of columns) {
				const raw = r[c.key];
				const cell = c.render ? c.render(raw, r) : (raw === undefined || raw === null ? '' : raw);
				body += '<td class="' + (c.num ? 'num ' : '') + (c.cls || '') + '">' + cell + '</td>';
			}
			body += '</tr>';
		}
		body += '</tbody>';

		table.innerHTML = head + body;
		count.textContent = data.length + ' of ' + rows.length;

		table.querySelectorAll('th').forEach(th => {
			th.addEventListener('click', () => {
				const key = th.dataset.key;
				if (state.sort === key) state.dir = -state.dir;
				else { state.sort = key; state.dir = 1; }
				draw();
			});
		});
	}

	draw();
	return { redraw: draw };
}

/* ---------- shared cell renderers ---------- */
const TIER_NAMES = ['Normal', 'Nightmare', 'Hell', 'Torment'];

function tierOfLevel(level) {
	if (level <= 0) return 0;
	return Math.min(3, Math.floor((level - 1) / 24));
}

function tierTag(index) {
	const name = TIER_NAMES[index] || 'Normal';
	return '<span class="tag t-' + name.toLowerCase() + '">' + name + '</span>';
}

function yesNo(value) {
	return value ? '<span class="tag ok">yes</span>' : '<span class="tag no">no</span>';
}

function areaOfLevel(level) {
	if (level <= 0) return '';
	const floor = ((level - 1) % 24) + 1;
	const areas = ['Cathedral', 'Catacombs', 'Caves', 'Hell', 'Nest', 'Crypt'];
	return TIER_NAMES[tierOfLevel(level)] + ' ' + areas[Math.floor((floor - 1) / 4)];
}

document.addEventListener('DOMContentLoaded', buildNav);
