#!/usr/bin/env node
/**
 * tools/oracool_mpq_pack.js - packs oracool.mpq and devilutionx.mpq without a freshly built program.
 *
 * WHY (2026-09-26): Windows 11's Smart App Control (the error says "blocked by your organization's Device Guard
 * policy", code 4551) refuses unsigned programs it has no reputation for, and every build links a brand-new
 * oracool_mpq_pack.exe. It refused that packer three builds running, so new art and sound never reached the
 * game. node.exe is signed and runs; this script writes the SAME archive the C++ packer writes, so the build
 * no longer depends on Windows' opinion of a new binary. The C++ packer (tools/oracool_mpq_pack.cpp) stays as
 * the fallback where node is absent.
 *
 * The format is the engine's own MpqWriter (Source/mpq/mpq_writer.cpp): a 104-byte header, a 2048-entry block
 * table and a 2048-entry hash table (both encrypted), then each file as 4096-byte sectors behind a sector offset
 * table, flagged Exists|PkZip. Every sector is stored RAW - which the reader already expects: MpqWriter itself
 * stores any sector implode could not shrink raw, and the reader copies a sector whose packed size equals its
 * unpacked size. Files are never encrypted.
 *
 * After writing, the archive is read back from disk and every entry compared byte for byte with its source,
 * and only then moved into place - the previous archive is untouched on any failure.
 *
 * Usage (identical to the C++ packer):
 *   node oracool_mpq_pack.js [--verify] <source_dir> <output.mpq> (<relative_file>... | @listfile)
 */
'use strict';
const fs = require('fs');
const path = require('path');

const HashEntries = 2048;
const BlockEntries = 2048;
const HeaderSize = 104; // sizeof(MpqFileHeader): 32 bytes + 72 of padding
const BlockTableOffset = HeaderSize;
const HashTableOffset = BlockTableOffset + BlockEntries * 16;
const DataOffset = HashTableOffset + HashEntries * 16;
const SectorSize = 4096; // 512 << 3
const FlagExists = 0x80000000;
const FlagPkZip = 0x00000100;

// ---- the crypt table and Hash(), exactly encrypt.cpp's -----------------------------------------------------
const cryptTable = (() => {
	const t = [0, 1, 2, 3, 4].map(() => new Uint32Array(256));
	let seed = 0x00100001;
	for (let i = 0; i < 256; i++) {
		for (let j = 0; j < 5; j++) {
			seed = (125 * seed + 3) % 0x2AAAAB;
			const hi = seed & 0xFFFF;
			seed = (125 * seed + 3) % 0x2AAAAB;
			t[j][i] = ((hi << 16) | (seed & 0xFFFF)) >>> 0;
		}
	}
	return t;
})();

function hash(s, type) {
	let seed1 = 0x7FED7FED;
	let seed2 = 0xEEEEEEEE;
	for (let i = 0; i < s.length; i++) {
		let ch = s.charCodeAt(i);
		if (ch >= 0x80)
			throw new Error(`non-ASCII archive name "${s}" - libmpq faults on it`);
		if (ch >= 0x61 && ch <= 0x7A)
			ch -= 0x20; // toupper, ASCII only
		seed1 = (cryptTable[type][ch] ^ ((seed1 + seed2) >>> 0)) >>> 0;
		seed2 = (ch + seed1 + seed2 + ((seed2 << 5) >>> 0) + 3) >>> 0;
	}
	return seed1;
}

function encrypt(buf, key) {
	let seed = 0xEEEEEEEE;
	for (let o = 0; o < buf.length; o += 4) {
		const ch = buf.readUInt32LE(o);
		seed = (seed + cryptTable[4][key & 0xFF]) >>> 0;
		buf.writeUInt32LE((ch ^ ((seed + key) >>> 0)) >>> 0, o);
		seed = (seed + ch + ((seed << 5) >>> 0) + 3) >>> 0;
		key = (((((key << 0x15) >>> 0) ^ 0xFFE00000) + 0x11111111) >>> 0 | (key >>> 0x0B)) >>> 0;
	}
}

function decrypt(buf, key) {
	let seed = 0xEEEEEEEE;
	for (let o = 0; o < buf.length; o += 4) {
		seed = (seed + cryptTable[4][key & 0xFF]) >>> 0;
		const t = (buf.readUInt32LE(o) ^ ((seed + key) >>> 0)) >>> 0;
		buf.writeUInt32LE(t, o);
		seed = (seed + t + ((seed << 5) >>> 0) + 3) >>> 0;
		key = (((((key << 0x15) >>> 0) ^ 0xFFE00000) + 0x11111111) >>> 0 | (key >>> 0x0B)) >>> 0;
	}
}

// ---- arguments ----------------------------------------------------------------------------------------------
function parseArgs(argv) {
	let i = 0;
	let verifyOnly = false;
	if (argv[i] === '--verify') {
		verifyOnly = true;
		i++;
	}
	if (argv.length - i < 3)
		throw new Error('usage: oracool_mpq_pack.js [--verify] <source_dir> <output.mpq> (<relative_file>... | @listfile)');
	const sourceDir = argv[i++];
	const outPath = argv[i++];
	let rel = argv.slice(i);
	if (rel.length === 1 && rel[0].startsWith('@')) {
		const text = fs.readFileSync(rel[0].slice(1), 'utf8').replace(/^﻿/, '');
		rel = text.split(/\r?\n/).map(l => l.trim()).filter(l => l.length > 0);
		if (rel.length === 0)
			throw new Error(`list file ${argv[i]} named no files`);
	}
	for (const r of rel) {
		if (r.includes('..') || r.startsWith('/') || r.startsWith('\\') || /^[A-Za-z]:/.test(r))
			throw new Error(`refusing path outside the source dir: ${r}`);
	}
	return { verifyOnly, sourceDir, outPath, rel };
}

const archiveName = rel => rel.replace(/\//g, '\\');

// ---- writing ------------------------------------------------------------------------------------------------
function buildArchive(sourceDir, rel) {
	if (rel.length > BlockEntries - 1)
		throw new Error(`${rel.length} files do not fit the ${BlockEntries}-entry tables`);
	const hashTable = Buffer.alloc(HashEntries * 16, 0xFF);
	const blockTable = Buffer.alloc(BlockEntries * 16, 0);
	const chunks = [];
	let offset = DataOffset;

	rel.forEach((r, blockIndex) => {
		const name = archiveName(r);
		const data = fs.readFileSync(path.join(sourceDir, r));
		const h1 = hash(name, 0);
		const h2 = hash(name, 1);
		const h3 = hash(name, 2);
		// Same probe as MpqWriter::AddFile: start at h1 & 0x7FF, walk to the first free slot.
		let idx = h1 & (HashEntries - 1);
		for (let n = 0; ; n++) {
			if (n === HashEntries)
				throw new Error('out of hash space');
			const at = idx * 16;
			const block = hashTable.readUInt32LE(at + 12);
			if (block === 0xFFFFFFFF || block === 0xFFFFFFFE)
				break;
			if (hashTable.readUInt32LE(at) === h2 && hashTable.readUInt32LE(at + 4) === h3)
				throw new Error(`hash collision between "${name}" and an existing file`);
			idx = (idx + 1) & (HashEntries - 1);
		}
		hashTable.writeUInt32LE(h2, idx * 16);
		hashTable.writeUInt32LE(h3, idx * 16 + 4);
		hashTable.writeUInt16LE(0, idx * 16 + 8);
		hashTable.writeUInt16LE(0, idx * 16 + 10);
		hashTable.writeUInt32LE(blockIndex, idx * 16 + 12);

		// Sector offset table, then the sectors raw - exactly what MpqWriter writes for incompressible data.
		const sectors = Math.max(1, Math.ceil(data.length / SectorSize));
		const tableBytes = 4 * (sectors + 1);
		const offsets = Buffer.alloc(tableBytes);
		let pos = tableBytes;
		for (let s = 0; s < sectors; s++) {
			offsets.writeUInt32LE(pos, 4 * s);
			pos += Math.min(SectorSize, data.length - s * SectorSize);
		}
		offsets.writeUInt32LE(pos, 4 * sectors);
		const packed = tableBytes + data.length;

		blockTable.writeUInt32LE(offset, blockIndex * 16);
		blockTable.writeUInt32LE(packed, blockIndex * 16 + 4);
		blockTable.writeUInt32LE(data.length, blockIndex * 16 + 8);
		blockTable.writeUInt32LE((FlagExists | FlagPkZip) >>> 0, blockIndex * 16 + 12);
		chunks.push(offsets, data);
		offset += packed;
	});

	const header = Buffer.alloc(HeaderSize, 0);
	header.write('MPQ\x1A', 0, 'latin1');
	header.writeUInt32LE(32, 4);
	header.writeUInt32LE(offset, 8); // the whole archive
	header.writeUInt16LE(0, 12);
	header.writeUInt16LE(3, 14); // block size factor: 512 << 3
	header.writeUInt32LE(HashTableOffset, 16);
	header.writeUInt32LE(BlockTableOffset, 20);
	header.writeUInt32LE(HashEntries, 24);
	header.writeUInt32LE(BlockEntries, 28);
	encrypt(blockTable, hash('(block table)', 3));
	encrypt(hashTable, hash('(hash table)', 3));
	return Buffer.concat([header, blockTable, hashTable, ...chunks]);
}

// ---- reading back (verification) ----------------------------------------------------------------------------
function readEntry(archive, name) {
	const hashTable = Buffer.from(archive.subarray(archive.readUInt32LE(16), archive.readUInt32LE(16) + HashEntries * 16));
	const blockTable = Buffer.from(archive.subarray(archive.readUInt32LE(20), archive.readUInt32LE(20) + BlockEntries * 16));
	decrypt(hashTable, hash('(hash table)', 3));
	decrypt(blockTable, hash('(block table)', 3));
	const h1 = hash(name, 0);
	const h2 = hash(name, 1);
	const h3 = hash(name, 2);
	let idx = h1 & (HashEntries - 1);
	for (let n = 0; n < HashEntries; n++) {
		const at = idx * 16;
		const block = hashTable.readUInt32LE(at + 12);
		if (block === 0xFFFFFFFF)
			return null;
		if (hashTable.readUInt32LE(at) === h2 && hashTable.readUInt32LE(at + 4) === h3) {
			const b = block * 16;
			const off = blockTable.readUInt32LE(b);
			const size = blockTable.readUInt32LE(b + 8);
			const sectors = Math.max(1, Math.ceil(size / SectorSize));
			const out = [];
			for (let s = 0; s < sectors; s++) {
				const from = archive.readUInt32LE(off + 4 * s);
				const to = archive.readUInt32LE(off + 4 * (s + 1));
				const want = Math.min(SectorSize, size - s * SectorSize);
				if (to - from !== want)
					throw new Error(`${name}: sector ${s} is ${to - from} bytes, expected ${want} raw`);
				out.push(archive.subarray(off + from, off + to));
			}
			return Buffer.concat(out);
		}
		idx = (idx + 1) & (HashEntries - 1);
	}
	return null;
}

function verify(archivePath, sourceDir, rel) {
	const archive = fs.readFileSync(archivePath);
	if (archive.toString('latin1', 0, 4) !== 'MPQ\x1A' || archive.readUInt32LE(8) !== archive.length)
		throw new Error(`${archivePath}: not a whole MPQ archive`);
	for (const r of rel) {
		const got = readEntry(archive, archiveName(r));
		if (got === null)
			throw new Error(`${archiveName(r)} is missing from ${archivePath}`);
		if (!got.equals(fs.readFileSync(path.join(sourceDir, r))))
			throw new Error(`${archiveName(r)} differs from its source`);
	}
}

// ---- main ---------------------------------------------------------------------------------------------------
function main() {
	const { verifyOnly, sourceDir, outPath, rel } = parseArgs(process.argv.slice(2));
	if (verifyOnly) {
		verify(outPath, sourceDir, rel);
		console.log(`verified ${rel.length} file(s) in ${outPath}`);
		return;
	}
	const bytes = buildArchive(sourceDir, rel);
	const temp = `${outPath}.${process.pid}.${Date.now()}.building`;
	fs.writeFileSync(temp, bytes);
	try {
		verify(temp, sourceDir, rel);
	} catch (e) {
		fs.rmSync(temp, { force: true });
		throw new Error(`the archive just written does not match its sources: ${e.message}`);
	}
	fs.renameSync(temp, outPath); // atomic on the same volume; the previous archive stays until this succeeds
	console.log(`packed ${rel.length} file(s), ${bytes.length} bytes -> ${outPath}`);
}

try {
	main();
} catch (e) {
	console.error(`ERROR: ${e.message} - the previous archive is untouched`);
	process.exit(1);
}
