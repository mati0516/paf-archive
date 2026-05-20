// PAF header/index parser — reads the binary format directly in JS.
// No WASM required for metadata; WASM is used only for extraction.
//
// Format:
//   Header 32B: magic(4) version(4) flags(4) file_count(4)
//               index_offset(8) path_offset(8)
//   Index N×128B: pb_offset(8) path_len(4) flags(4)
//                 data_offset(8) data_size(8) hash[64] reserved[32]
//   Path Buffer: raw UTF-8 (no NUL)
//
// PAF_FLAG_INDEX_ONLY = 0x02 — data block absent; verification not possible.

const PAF_FLAG_INDEX_ONLY  = 0x02;
const PAF_ENTRY_DELETED    = 0x04;
const INDEX_ENTRY_SIZE     = 128;

function parsePafIndex(buffer) {
    const view = new DataView(buffer);

    const magic = String.fromCharCode(
        view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3)
    );
    if (magic !== 'PAF1') throw new Error('Not a valid PAF archive (bad magic)');

    const headerFlags  = view.getUint32(8, true);
    const fileCount    = view.getUint32(12, true);
    const indexOffset  = readU64(view, 16);
    const pathOffset   = readU64(view, 24);
    const indexOnly    = !!(headerFlags & PAF_FLAG_INDEX_ONLY);

    const decoder = new TextDecoder('utf-8');
    const files = [];

    for (let i = 0; i < fileCount; i++) {
        const base = indexOffset + i * INDEX_ENTRY_SIZE;

        const pbOffset  = readU64(view, base + 0);
        const pathLen   = view.getUint32(base + 8, true);
        const entryFlags = view.getUint32(base + 12, true);
        const dataOffset = readU64(view, base + 16);
        const dataSize   = readU64(view, base + 24);

        const hashBytes = new Uint8Array(buffer, base + 32, 32);
        const hashHex   = bytesToHex(hashBytes);

        const pathBytes = new Uint8Array(buffer, pathOffset + pbOffset, pathLen);
        const path      = decoder.decode(pathBytes);

        const deleted = !!(entryFlags & PAF_ENTRY_DELETED);

        files.push({ path, size: dataSize, dataOffset, hashHex, deleted, indexOnly });
    }

    return { files, indexOnly };
}

function readU64(view, offset) {
    const lo = view.getUint32(offset, true);
    const hi = view.getUint32(offset + 4, true);
    return lo + hi * 0x100000000;
}

function bytesToHex(bytes) {
    return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
}

// ---- Main app ---------------------------------------------------------------

document.addEventListener('DOMContentLoaded', () => {
    if (typeof initI18n === 'function') initI18n();

    // Tabs
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.add('hidden'));
            btn.classList.add('active');
            document.getElementById(btn.dataset.target).classList.remove('hidden');
        });
    });

    // ---- VIEWER ----
    const exDropZone  = document.getElementById('extract-drop-zone');
    const exFileInput = document.getElementById('extract-file-input');
    const exResults   = document.getElementById('extract-results');
    const fileListBody = document.getElementById('file-list');
    const exLoading   = document.getElementById('extract-loading');
    const exStatus    = document.getElementById('extract-status');
    const verifyBtn   = document.getElementById('verify-btn');

    let currentArchiveName = '';
    let currentBuffer = null;   // raw bytes of the loaded .paf
    let parsedFiles   = [];     // [{path, size, dataOffset, hashHex, deleted, indexOnly}]

    setupDragAndDrop(exDropZone, files => { if (files.length) handleExtractFile(files[0]); });
    exDropZone.addEventListener('click', () => exFileInput.click());
    exFileInput.addEventListener('change', e => { if (e.target.files.length) handleExtractFile(e.target.files[0]); });

    function handleExtractFile(file) {
        if (!file.name.endsWith('.paf')) { alert(t('alert_need_paf')); return; }

        exLoading.classList.remove('hidden');
        exResults.classList.add('hidden');
        exStatus.innerText = '';
        currentArchiveName = file.name;
        currentBuffer = null;
        parsedFiles = [];

        const reader = new FileReader();
        reader.onload = e => {
            currentBuffer = e.target.result;
            try {
                const { files, indexOnly } = parsePafIndex(currentBuffer);
                parsedFiles = files;
                renderFileList(files);
                verifyBtn.disabled = indexOnly;
                verifyBtn.title = indexOnly ? t('verify_index_only') : '';
                exLoading.classList.add('hidden');
                exResults.classList.remove('hidden');

                // Also write to WASM FS so extraction still works
                if (typeof Module !== 'undefined' && Module.FS) {
                    Module.FS.writeFile('/' + file.name, new Uint8Array(currentBuffer));
                } else if (typeof Module !== 'undefined') {
                    Module.onRuntimeInitialized = () => {
                        Module.FS.writeFile('/' + file.name, new Uint8Array(currentBuffer));
                    };
                }
            } catch (err) {
                exLoading.classList.add('hidden');
                alert(t('alert_read_err') + '\n' + err.message);
            }
        };
        reader.readAsArrayBuffer(file);
    }

    function renderFileList(files) {
        fileListBody.innerHTML = '';
        if (!files.length) {
            fileListBody.innerHTML = `<tr><td colspan="4">${t('empty_archive')}</td></tr>`;
            return;
        }
        files.forEach((f, idx) => {
            const tr = document.createElement('tr');
            tr.dataset.idx = idx;
            const shortHash = f.hashHex.slice(0, 8) + '…';
            const sizeStr = f.deleted ? `<em style="color:#64748b">${t('entry_deleted')}</em>` : f.size.toLocaleString();
            tr.innerHTML = `
                <td>${escapeHtml(f.path)}</td>
                <td>${sizeStr}</td>
                <td class="hash-cell" title="${f.hashHex}">${f.indexOnly && !f.deleted ? '<span class="status-skip">—</span>' : shortHash}</td>
                <td class="status-cell">—</td>`;
            fileListBody.appendChild(tr);
        });
    }

    // ---- Integrity verification ----
    verifyBtn.addEventListener('click', async () => {
        if (!currentBuffer || !parsedFiles.length) return;

        verifyBtn.disabled = true;
        exStatus.innerText = t('verify_running');

        let okCount = 0, failCount = 0, skipCount = 0;
        const rows = fileListBody.querySelectorAll('tr[data-idx]');

        for (const row of rows) {
            const idx = parseInt(row.dataset.idx, 10);
            const f   = parsedFiles[idx];
            const cell = row.querySelector('.status-cell');

            if (f.deleted || f.size === 0) {
                cell.innerHTML = `<span class="status-skip">${t('verify_skip')}</span>`;
                skipCount++;
                continue;
            }

            try {
                const slice     = currentBuffer.slice(f.dataOffset, f.dataOffset + f.size);
                const hashBuf   = await crypto.subtle.digest('SHA-256', slice);
                const computed  = bytesToHex(new Uint8Array(hashBuf));
                const ok        = computed === f.hashHex;

                if (ok) {
                    cell.innerHTML = `<span class="status-ok">✓</span>`;
                    okCount++;
                } else {
                    cell.innerHTML = `<span class="status-fail">✗</span>`;
                    failCount++;
                }
            } catch {
                cell.innerHTML = `<span class="status-fail">err</span>`;
                failCount++;
            }
        }

        verifyBtn.disabled = false;

        if (failCount === 0) {
            exStatus.innerText = t('verify_all_ok').replace('{n}', okCount + skipCount);
        } else {
            exStatus.style.color = '#ef4444';
            exStatus.innerText = t('verify_has_errors').replace('{f}', failCount).replace('{n}', okCount + failCount + skipCount);
        }
    });

    // ---- Extract to PC ----
    const exLocalBtn = document.getElementById('extract-local-btn');
    if (exLocalBtn) {
        if (!window.showDirectoryPicker) {
            exLocalBtn.innerText = 'Unsupported Browser (Use Chrome/Edge)';
            exLocalBtn.disabled = true;
        } else {
            exLocalBtn.addEventListener('click', async () => {
                if (!currentArchiveName || !currentBuffer) return;
                try {
                    const dirHandle = await window.showDirectoryPicker({ mode: 'readwrite' });
                    exLoading.classList.remove('hidden');

                    const outDir = '/out/';
                    try { Module.FS.mkdir(outDir); } catch (e) {}

                    const c_path = Module.stringToNewUTF8('/' + currentArchiveName);
                    const c_out  = Module.stringToNewUTF8(outDir);
                    const res    = Module._wasm_paf_extract_all(c_path, c_out);
                    Module._free(c_path);
                    Module._free(c_out);

                    if (res !== 0) {
                        exStatus.innerText = t('extract_fail');
                        exLoading.classList.add('hidden');
                        return;
                    }

                    await copyVirtualDirToLocal(outDir, dirHandle);
                    exStatus.style.color = '';
                    exStatus.innerText = t('extract_local_success');
                } catch (err) {
                    if (err.name !== 'AbortError') exStatus.innerText = '❌ ' + err.message;
                } finally {
                    exLoading.classList.add('hidden');
                }
            });
        }
    }

    async function copyVirtualDirToLocal(vPath, localDirHandle) {
        for (const item of Module.FS.readdir(vPath)) {
            if (item === '.' || item === '..') continue;
            const itemPath = vPath + item;
            const stat     = Module.FS.stat(itemPath);
            if (Module.FS.isDir(stat.mode)) {
                const sub = await localDirHandle.getDirectoryHandle(item, { create: true });
                await copyVirtualDirToLocal(itemPath + '/', sub);
            } else {
                const fh  = await localDirHandle.getFileHandle(item, { create: true });
                const writable = await fh.createWritable();
                await writable.write(Module.FS.readFile(itemPath));
                await writable.close();
            }
        }
    }

    // ---- PACKAGE ----
    const cmpDropZone  = document.getElementById('compress-drop-zone');
    const cmpFileInput = document.getElementById('compress-file-input');
    const cmpResults   = document.getElementById('compress-results');
    const cmpLoading   = document.getElementById('compress-loading');
    const cmpPafBtn    = document.getElementById('compress-paf-btn');
    const cmpCount     = document.getElementById('compress-file-count');
    const dlLink       = document.getElementById('download-link');

    let filesToCompress = [];

    setupDragAndDrop(cmpDropZone, files => { if (files.length) handleCompressFiles(files); });
    cmpDropZone.addEventListener('click', () => cmpFileInput.click());
    cmpFileInput.addEventListener('change', e => { if (e.target.files.length) handleCompressFiles(e.target.files); });

    function handleCompressFiles(fileList) {
        filesToCompress = Array.from(fileList);
        cmpCount.innerText = filesToCompress.length;
        cmpResults.classList.remove('hidden');
    }

    cmpPafBtn.addEventListener('click', async () => {
        if (!filesToCompress.length) return;
        cmpLoading.classList.remove('hidden');

        if (typeof Module === 'undefined' || !Module.FS || typeof Module._wasm_paf_create !== 'function') {
            alert(t('alert_wasm_err'));
            cmpLoading.classList.add('hidden');
            return;
        }

        try {
            const inputDir = '/paf_input';
            try { Module.FS.mkdir(inputDir); } catch (e) {}

            for (const file of filesToCompress) {
                const data = new Uint8Array(await file.arrayBuffer());
                Module.FS.writeFile(inputDir + '/' + file.name, data);
            }

            const outArchive = '/created.paf';
            const c_out = Module.stringToNewUTF8(outArchive);
            const c_dir = Module.stringToNewUTF8(inputDir);
            const res   = Module._wasm_paf_create(c_out, c_dir);
            Module._free(c_out);
            Module._free(c_dir);

            if (res === 0) {
                triggerDownload(Module.FS.readFile(outArchive), 'archive.paf', 'application/octet-stream');
            } else {
                alert(t('alert_paf_err'));
            }
        } catch (e) {
            console.error(e);
            alert(t('alert_paf_err'));
        } finally {
            cmpLoading.classList.add('hidden');
        }
    });

    function triggerDownload(dataArray, filename, mimeType) {
        const blob = new Blob([dataArray], { type: mimeType });
        const url  = URL.createObjectURL(blob);
        const a    = document.createElement('a');
        a.href = url; a.download = filename;
        document.body.appendChild(a); a.click(); document.body.removeChild(a);

        dlLink.href = url; dlLink.download = filename;
        dlLink.removeAttribute('hidden');
        dlLink.style.cssText = 'display:inline-block;margin-top:1rem;padding:0.5rem 1rem;background:#3b82f6;color:#fff;border-radius:6px;text-decoration:none;font-weight:600';
        dlLink.textContent = `📥 ${filename}`;
        setTimeout(() => URL.revokeObjectURL(url), 60000);
    }

    // ---- Utils ----
    function setupDragAndDrop(el, cb) {
        el.addEventListener('dragover',  e => { e.preventDefault(); el.classList.add('dragover'); });
        el.addEventListener('dragleave', ()  => el.classList.remove('dragover'));
        el.addEventListener('drop',      e  => { e.preventDefault(); el.classList.remove('dragover'); cb(e.dataTransfer.files); });
    }

    function escapeHtml(str) {
        return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    }
});
