document.addEventListener('DOMContentLoaded', () => {
    // ---- i18n Logic ----
    if (typeof initI18n === 'function') {
        initI18n();
    }

    // ---- Tabs Logic ----
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            tabBtns.forEach(b => b.classList.remove('active'));
            tabContents.forEach(c => c.classList.add('hidden'));

            btn.classList.add('active');
            document.getElementById(btn.dataset.target).classList.remove('hidden');
        });
    });

    // ---- VIEWER (閲覧/展開) LOGIC ----
    const exDropZone = document.getElementById('extract-drop-zone');
    const exFileInput = document.getElementById('extract-file-input');
    const exResults = document.getElementById('extract-results');
    const fileListBody = document.getElementById('file-list');
    const exLoading = document.getElementById('extract-loading');
    const exStatus = document.getElementById('extract-status');

    let currentArchiveName = "";

    exDropZone.addEventListener('click', () => exFileInput.click());
    setupDragAndDrop(exDropZone, (files) => {
        if (files.length > 0) handleExtractFile(files[0]);
    });
    exFileInput.addEventListener('change', (e) => {
        if (e.target.files.length) handleExtractFile(e.target.files[0]);
    });

    window.paf_on_list = (jsonString) => {
        try {
            const files = JSON.parse(jsonString);
            renderFileList(files);
            exLoading.classList.add('hidden');
            exResults.classList.remove('hidden');
        } catch (err) {
            console.error(err);
            alert(t("alert_parse_err"));
        }
    };

    function handleExtractFile(file) {
        if (!file.name.endsWith('.paf')) {
            alert(t("alert_need_paf"));
            return;
        }
        exLoading.classList.remove('hidden');
        exResults.classList.add('hidden');
        exStatus.innerText = "";
        currentArchiveName = file.name;

        const reader = new FileReader();
        reader.onload = function(e) {
            const data = new Uint8Array(e.target.result);
            if (typeof Module !== 'undefined' && Module.FS) {
                processExtract(data, file.name);
            } else {
                Module.onRuntimeInitialized = () => processExtract(data, file.name);
            }
        };
        reader.readAsArrayBuffer(file);
    }

    function processExtract(data, filename) {
        try {
            const virtualPath = '/' + filename;
            Module.FS.writeFile(virtualPath, data);
            
            const c_path = Module.stringToNewUTF8(virtualPath);
            const res = Module._wasm_paf_list(c_path);
            Module._free(c_path);
            
            if (res !== 0) {
                alert(t("alert_read_err"));
                exLoading.classList.add('hidden');
            }
        } catch (e) {
            console.error(e);
            alert("WASM Error");
        }
    }

    function renderFileList(files) {
        fileListBody.innerHTML = '';
        if (files.length === 0) {
            fileListBody.innerHTML = `<tr><td colspan="2">${t("empty_archive")}</td></tr>`;
            return;
        }
        files.forEach(f => {
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>${f.path}</td><td>${f.size.toLocaleString()}</td>`;
            fileListBody.appendChild(tr);
        });
    }

    const exLocalBtn = document.getElementById('extract-local-btn');
    if (exLocalBtn) {
        if (!window.showDirectoryPicker) {
            exLocalBtn.innerText = "Unsupported Browser (Use Chrome/Edge)";
            exLocalBtn.disabled = true;
        } else {
            exLocalBtn.addEventListener('click', async () => {
                if (!currentArchiveName) return;
                
                try {
                    // 1. ユーザーに保存先フォルダを選択させる
                    const dirHandle = await window.showDirectoryPicker({
                        mode: 'readwrite'
                    });
                    
                    exLoading.classList.remove('hidden');
                    
                    // 2. 仮想FSに一旦すべて展開
                    const outDir = '/out/';
                    try { Module.FS.mkdir(outDir); } catch(e) {}
                    const c_path = Module.stringToNewUTF8('/' + currentArchiveName);
                    const c_out = Module.stringToNewUTF8(outDir);
                    const res = Module._wasm_paf_extract_all(c_path, c_out);
                    Module._free(c_path); Module._free(c_out);
                    
                    if (res !== 0) {
                        exStatus.innerText = t("extract_fail");
                        exLoading.classList.add('hidden');
                        return;
                    }

                    // 3. 仮想FS (/out/) から PCのフォルダへ再帰的にコピー
                    await copyVirtualDirToLocal(outDir, dirHandle);
                    
                    exStatus.innerText = t("extract_local_success") || "✅ Successfully saved to PC folder.";
                } catch (err) {
                    console.error(err);
                    if (err.name !== 'AbortError') {
                        exStatus.innerText = "❌ " + err.message;
                    }
                } finally {
                    exLoading.classList.add('hidden');
                }
            });
        }
    }

    // 仮想FSのディレクトリを再帰的にローカルへコピーする関数
    async function copyVirtualDirToLocal(vPath, localDirHandle) {
        const items = Module.FS.readdir(vPath);
        for (let item of items) {
            if (item === '.' || item === '..') continue;
            
            const itemPath = vPath + item;
            const stat = Module.FS.stat(itemPath);
            
            if (Module.FS.isDir(stat.mode)) {
                // サブディレクトリ作成
                const subDirHandle = await localDirHandle.getDirectoryHandle(item, { create: true });
                await copyVirtualDirToLocal(itemPath + '/', subDirHandle);
            } else {
                // ファイル書き込み
                const fileData = Module.FS.readFile(itemPath);
                const fileHandle = await localDirHandle.getFileHandle(item, { create: true });
                const writable = await fileHandle.createWritable();
                await writable.write(fileData);
                await writable.close();
            }
        }
    }

    // ---- PACKAGE (まとめる) LOGIC ----
    const cmpDropZone = document.getElementById('compress-drop-zone');
    const cmpFileInput = document.getElementById('compress-file-input');
    const cmpResults = document.getElementById('compress-results');
    const cmpLoading = document.getElementById('compress-loading');
    const cmpPafBtn = document.getElementById('compress-paf-btn');

    const cmpCount = document.getElementById('compress-file-count');
    const dlLink = document.getElementById('download-link');

    let filesToCompress = [];

    cmpDropZone.addEventListener('click', () => cmpFileInput.click());
    setupDragAndDrop(cmpDropZone, (files) => {
        if (files.length > 0) handleCompressFiles(files);
    });
    cmpFileInput.addEventListener('change', (e) => {
        if (e.target.files.length) handleCompressFiles(e.target.files);
    });

    function handleCompressFiles(fileList) {
        filesToCompress = Array.from(fileList);
        cmpCount.innerText = filesToCompress.length;
        cmpResults.classList.remove('hidden');
    }

    // .paf 作成処理
    cmpPafBtn.addEventListener('click', async () => {
        if (filesToCompress.length === 0) return;
        cmpLoading.classList.remove('hidden');
        
        if (typeof Module === 'undefined' || !Module.FS || typeof Module._wasm_paf_create !== 'function') {
            alert(t("alert_wasm_err") + ' (WASM not built with paf_create. Please rebuild.)');
            cmpLoading.classList.add('hidden');
            return;
        }

        try {
            // 一時入力ディレクトリを仮想FS上に作成
            const inputDir = '/paf_input';
            try { Module.FS.mkdir(inputDir); } catch(e) {}

            // 全ファイルを /paf_input/ に書き込む
            for (let i = 0; i < filesToCompress.length; i++) {
                const file = filesToCompress[i];
                const arrayBuffer = await file.arrayBuffer();
                const data = new Uint8Array(arrayBuffer);
                Module.FS.writeFile(inputDir + '/' + file.name, data);
            }

            const outArchive = '/created.paf';
            const c_out = Module.stringToNewUTF8(outArchive);
            const c_dir = Module.stringToNewUTF8(inputDir);
            
            const res = Module._wasm_paf_create(c_out, c_dir);
            
            Module._free(c_out);
            Module._free(c_dir);

            if (res === 0) {
                const pafData = Module.FS.readFile(outArchive);
                triggerDownload(pafData, 'archive.paf', 'application/octet-stream');
            } else {
                alert(t("alert_paf_err"));
            }
        } catch (e) {
            console.error(e);
            alert(t("alert_paf_err"));
        } finally {
            cmpLoading.classList.add('hidden');
        }
    });


    function triggerDownload(dataArray, filename, mimeType) {
        const blob = new Blob([dataArray], { type: mimeType });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(() => URL.revokeObjectURL(url), 1000);
    }


    // ---- Utils ----
    function setupDragAndDrop(element, callback) {
        element.addEventListener('dragover', (e) => { e.preventDefault(); element.classList.add('dragover'); });
        element.addEventListener('dragleave', () => element.classList.remove('dragover'));
        element.addEventListener('drop', (e) => {
            e.preventDefault();
            element.classList.remove('dragover');
            callback(e.dataTransfer.files);
        });
    }
});
