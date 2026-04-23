const translations = {
    "en": {
        "title": "📦 PAF Web Tool",
        "subtitle": "High-speed parallel archive format",
        "tab_viewer": "Viewer (Extract)",
        "tab_package": "Package (Create)",
        "drop_view": "Drop a .paf file here to view and extract",
        "processing": "Processing archive...",
        "archive_contents": "Archive Contents",
        "th_path": "File Path",
        "th_size": "Size (Bytes)",
        "extract_btn": "Extract All to Virtual FS",
        "drop_create": "Drop multiple files here to create an archive",
        "creating": "Creating archive...",
        "selected_files": "Selected Files",
        "btn_create_paf": "Create .paf",
        "btn_create_zip": "Create .zip (OS Mountable)",
        "empty_archive": "Archive is empty.",
        "extract_success": "✅ Successfully extracted to Virtual FS (/out/)",
        "extract_fail": "❌ Extraction failed.",
        "alert_need_paf": "Please select a .paf file for extraction.",
        "alert_parse_err": "Error parsing archive contents.",
        "alert_read_err": "Failed to read archive contents.",
        "alert_wasm_err": "WASM module not loaded yet.",
        "alert_paf_err": "Failed to create PAF archive.",
        "alert_zip_err": "Error during ZIP creation."
    },
    "ja": {
        "title": "📦 PAF Web ツール",
        "subtitle": "超高速パラレルアーカイブフォーマット",
        "tab_viewer": "閲覧 (Viewer)",
        "tab_package": "まとめる (Package)",
        "drop_view": "ここに .paf ファイルをドロップして閲覧",
        "processing": "アーカイブを処理中...",
        "archive_contents": "アーカイブの内容",
        "th_path": "ファイルパス",
        "th_size": "サイズ (バイト)",
        "extract_btn": "仮想FSにすべて展開",
        "drop_create": "ここに複数のファイルをドロップしてパッケージ化",
        "creating": "アーカイブを作成中...",
        "selected_files": "選択されたファイル",
        "btn_create_paf": ".paf を作成",
        "btn_create_zip": ".zip を作成 (OS標準マウント用)",
        "empty_archive": "アーカイブは空です。",
        "extract_success": "✅ 仮想FS (/out/) への展開に成功しました",
        "extract_fail": "❌ 展開に失敗しました。",
        "alert_need_paf": "閲覧するには .paf ファイルを選択してください。",
        "alert_parse_err": "アーカイブ内容の解析エラー。",
        "alert_read_err": "アーカイブ内容の読み込みに失敗しました。",
        "alert_wasm_err": "WASMモジュールがまだロードされていません。",
        "alert_paf_err": "PAFアーカイブの作成に失敗しました。",
        "alert_zip_err": "ZIP作成中にエラーが発生しました。"
    },
    "zh": {
        "title": "📦 PAF 网页工具",
        "subtitle": "高速并行存档格式",
        "tab_viewer": "查看器 (Viewer)",
        "tab_package": "打包 (Package)",
        "drop_view": "将 .paf 文件拖放到此处以查看",
        "processing": "正在处理存档...",
        "archive_contents": "存档内容",
        "th_path": "文件路径",
        "th_size": "大小 (字节)",
        "extract_btn": "全部提取到虚拟FS",
        "drop_create": "将多个文件拖放到此处以创建存档",
        "creating": "正在创建存档...",
        "selected_files": "选定的文件",
        "btn_create_paf": "创建 .paf",
        "btn_create_zip": "创建 .zip (操作系统可挂载)",
        "empty_archive": "存档为空。",
        "extract_success": "✅ 成功提取到虚拟FS (/out/)",
        "extract_fail": "❌ 提取失败。",
        "alert_need_paf": "请选择一个 .paf 文件进行查看。",
        "alert_parse_err": "解析存档内容时出错。",
        "alert_read_err": "读取存档内容失败。",
        "alert_wasm_err": "WASM模块尚未加载。",
        "alert_paf_err": "创建PAF存档失败。",
        "alert_zip_err": "创建ZIP期间出错。"
    },
    "es": {
        "title": "📦 Herramienta Web PAF",
        "subtitle": "Formato de archivo paralelo de alta velocidad",
        "tab_viewer": "Visor (Viewer)",
        "tab_package": "Empaquetar (Package)",
        "drop_view": "Suelta un archivo .paf aquí para verlo",
        "processing": "Procesando archivo...",
        "archive_contents": "Contenido del archivo",
        "th_path": "Ruta del archivo",
        "th_size": "Tamaño (Bytes)",
        "extract_btn": "Extraer todo a Virtual FS",
        "drop_create": "Suelta varios archivos aquí para crear un archivo",
        "creating": "Creando archivo...",
        "selected_files": "Archivos seleccionados",
        "btn_create_paf": "Crear .paf",
        "btn_create_zip": "Crear .zip (Montable en OS)",
        "empty_archive": "El archivo está vacío.",
        "extract_success": "✅ Extraído con éxito a Virtual FS (/out/)",
        "extract_fail": "❌ Falló la extracción.",
        "alert_need_paf": "Seleccione un archivo .paf para extraer.",
        "alert_parse_err": "Error al analizar el contenido del archivo.",
        "alert_read_err": "Error al leer el contenido del archivo.",
        "alert_wasm_err": "El módulo WASM aún no está cargado.",
        "alert_paf_err": "Error al crear el archivo PAF.",
        "alert_zip_err": "Error durante la creación del ZIP."
    },
    "hi": {
        "title": "📦 PAF वेब टूल",
        "subtitle": "हाई-स्पीड पैरेलल आर्काइव फॉर्मेट",
        "tab_viewer": "व्यूअर (Viewer)",
        "tab_package": "पैकेज (Package)",
        "drop_view": "देखने के लिए यहां .paf फाइल छोड़ें",
        "processing": "आर्काइव प्रोसेस हो रहा है...",
        "archive_contents": "आर्काइव सामग्री",
        "th_path": "फाइल पथ",
        "th_size": "आकार (बाइट्स)",
        "extract_btn": "वर्चुअल FS में सब निकालें",
        "drop_create": "आर्काइव बनाने के लिए कई फाइलें यहां छोड़ें",
        "creating": "आर्काइव बना रहा है...",
        "selected_files": "चयनित फाइलें",
        "btn_create_paf": ".paf बनाएं",
        "btn_create_zip": ".zip बनाएं (OS माउंटेबल)",
        "empty_archive": "आर्काइव खाली है।",
        "extract_success": "✅ वर्चुअल FS (/out/) में सफलतापूर्वक निकाला गया",
        "extract_fail": "❌ निष्कर्षण विफल।",
        "alert_need_paf": "कृपया देखने के लिए .paf फ़ाइल चुनें।",
        "alert_parse_err": "आर्काइव सामग्री को पार्स करने में त्रुटि।",
        "alert_read_err": "आर्काइव सामग्री पढ़ने में विफल।",
        "alert_wasm_err": "WASM मॉड्यूल अभी लोड नहीं हुआ है।",
        "alert_paf_err": "PAF आर्काइव बनाने में विफल।",
        "alert_zip_err": "ZIP निर्माण के दौरान त्रुटि।"
    },
    "ar": {
        "title": "📦 أداة الويب PAF",
        "subtitle": "تنسيق أرشيف متوازي عالي السرعة",
        "tab_viewer": "عارض (Viewer)",
        "tab_package": "حزمة (Package)",
        "drop_view": "قم بإفلات ملف .paf هنا لعرضه",
        "processing": "جاري معالجة الأرشيف...",
        "archive_contents": "محتويات الأرشيف",
        "th_path": "مسار الملف",
        "th_size": "الحجم (بايت)",
        "extract_btn": "استخراج الكل إلى Virtual FS",
        "drop_create": "قم بإفلات ملفات متعددة هنا لإنشاء أرشيف",
        "creating": "جاري إنشاء الأرشيف...",
        "selected_files": "الملفات المحددة",
        "btn_create_paf": "إنشاء .paf",
        "btn_create_zip": "إنشاء .zip (قابل للتثبيت على نظام التشغيل)",
        "empty_archive": "الأرشيف فارغ.",
        "extract_success": "✅ تم الاستخراج بنجاح إلى Virtual FS (/out/)",
        "extract_fail": "❌ فشل الاستخراج.",
        "alert_need_paf": "الرجاء تحديد ملف .paf للعرض.",
        "alert_parse_err": "خطأ في تحليل محتويات الأرشيف.",
        "alert_read_err": "فشلت قراءة محتويات الأرشيف.",
        "alert_wasm_err": "لم يتم تحميل وحدة WASM بعد.",
        "alert_paf_err": "فشل إنشاء أرشيف PAF.",
        "alert_zip_err": "خطأ أثناء إنشاء ZIP."
    },
    "fr": {
        "title": "📦 Outil Web PAF",
        "subtitle": "Format d'archive parallèle à haute vitesse",
        "tab_viewer": "Visionneuse (Viewer)",
        "tab_package": "Créer (Package)",
        "drop_view": "Déposez un fichier .paf ici pour le voir",
        "processing": "Traitement de l'archive...",
        "archive_contents": "Contenu de l'archive",
        "th_path": "Chemin du fichier",
        "th_size": "Taille (Octets)",
        "extract_btn": "Tout extraire vers Virtual FS",
        "drop_create": "Déposez plusieurs fichiers ici pour créer une archive",
        "creating": "Création de l'archive...",
        "selected_files": "Fichiers sélectionnés",
        "btn_create_paf": "Créer .paf",
        "btn_create_zip": "Créer .zip (Montable sur l'OS)",
        "empty_archive": "L'archive est vide.",
        "extract_success": "✅ Extrait avec succès vers Virtual FS (/out/)",
        "extract_fail": "❌ L'extraction a échoué.",
        "alert_need_paf": "Veuillez sélectionner un fichier .paf.",
        "alert_parse_err": "Erreur lors de l'analyse du contenu de l'archive.",
        "alert_read_err": "Échec de la lecture du contenu de l'archive.",
        "alert_wasm_err": "Le module WASM n'est pas encore chargé.",
        "alert_paf_err": "Échec de la création de l'archive PAF.",
        "alert_zip_err": "Erreur lors de la création du ZIP."
    },
    "ru": {
        "title": "📦 Веб-инструмент PAF",
        "subtitle": "Высокоскоростной параллельный формат архива",
        "tab_viewer": "Просмотр (Viewer)",
        "tab_package": "Упаковка (Package)",
        "drop_view": "Перетащите файл .paf сюда для просмотра",
        "processing": "Обработка архива...",
        "archive_contents": "Содержимое архива",
        "th_path": "Путь к файлу",
        "th_size": "Размер (Байт)",
        "extract_btn": "Извлечь всё в Virtual FS",
        "drop_create": "Перетащите несколько файлов сюда для создания архива",
        "creating": "Создание архива...",
        "selected_files": "Выбранные файлы",
        "btn_create_paf": "Создать .paf",
        "btn_create_zip": "Создать .zip (Монтируемый в ОС)",
        "empty_archive": "Архив пуст.",
        "extract_success": "✅ Успешно извлечено в Virtual FS (/out/)",
        "extract_fail": "❌ Ошибка извлечения.",
        "alert_need_paf": "Пожалуйста, выберите файл .paf.",
        "alert_parse_err": "Ошибка анализа содержимого архива.",
        "alert_read_err": "Не удалось прочитать содержимое архива.",
        "alert_wasm_err": "Модуль WASM еще не загружен.",
        "alert_paf_err": "Не удалось создать архив PAF.",
        "alert_zip_err": "Ошибка при создании ZIP."
    },
    "pt": {
        "title": "📦 Ferramenta Web PAF",
        "subtitle": "Formato de arquivo paralelo de alta velocidade",
        "tab_viewer": "Visualizador (Viewer)",
        "tab_package": "Empacotar (Package)",
        "drop_view": "Arraste um arquivo .paf aqui para ver",
        "processing": "Processando arquivo...",
        "archive_contents": "Conteúdo do arquivo",
        "th_path": "Caminho do arquivo",
        "th_size": "Tamanho (Bytes)",
        "extract_btn": "Extrair tudo para Virtual FS",
        "drop_create": "Arraste vários arquivos aqui para criar um arquivo",
        "creating": "Criando arquivo...",
        "selected_files": "Arquivos selecionados",
        "btn_create_paf": "Criar .paf",
        "btn_create_zip": "Criar .zip (Montável no SO)",
        "empty_archive": "O arquivo está vazio.",
        "extract_success": "✅ Extraído com sucesso para Virtual FS (/out/)",
        "extract_fail": "❌ Falha na extração.",
        "alert_need_paf": "Selecione um arquivo .paf.",
        "alert_parse_err": "Erro ao analisar o conteúdo do arquivo.",
        "alert_read_err": "Falha ao ler o conteúdo do arquivo.",
        "alert_wasm_err": "O módulo WASM ainda não foi carregado.",
        "alert_paf_err": "Falha ao criar o arquivo PAF.",
        "alert_zip_err": "Erro durante a criação do ZIP."
    },
    "de": {
        "title": "📦 PAF Web-Tool",
        "subtitle": "Paralleles Hochgeschwindigkeits-Archivformat",
        "tab_viewer": "Betrachter (Viewer)",
        "tab_package": "Verpacken (Package)",
        "drop_view": "Ziehen Sie eine .paf-Datei hierher zum Anzeigen",
        "processing": "Archiv wird verarbeitet...",
        "archive_contents": "Archivinhalt",
        "th_path": "Dateipfad",
        "th_size": "Größe (Bytes)",
        "extract_btn": "Alles nach Virtual FS extrahieren",
        "drop_create": "Ziehen Sie mehrere Dateien hierher, um ein Archiv zu erstellen",
        "creating": "Archiv wird erstellt...",
        "selected_files": "Ausgewählte Dateien",
        "btn_create_paf": "Erstellen .paf",
        "btn_create_zip": "Erstellen .zip (OS-Montierbar)",
        "empty_archive": "Archiv ist leer.",
        "extract_success": "✅ Erfolgreich nach Virtual FS extrahiert (/out/)",
        "extract_fail": "❌ Extraktion fehlgeschlagen.",
        "alert_need_paf": "Bitte wählen Sie eine .paf-Datei aus.",
        "alert_parse_err": "Fehler beim Parsen des Archivinhalts.",
        "alert_read_err": "Fehler beim Lesen des Archivinhalts.",
        "alert_wasm_err": "WASM-Modul noch nicht geladen.",
        "alert_paf_err": "Fehler beim Erstellen des PAF-Archivs.",
        "alert_zip_err": "Fehler bei der ZIP-Erstellung."
    },
    "ko": {
        "title": "📦 PAF 웹 도구",
        "subtitle": "초고속 병렬 아카이브 형식",
        "tab_viewer": "뷰어 (Viewer)",
        "tab_package": "패키지 (Package)",
        "drop_view": "여기에 .paf 파일을 놓으세요",
        "processing": "아카이브 처리 중...",
        "archive_contents": "아카이브 내용",
        "th_path": "파일 경로",
        "th_size": "크기 (바이트)",
        "extract_btn": "Virtual FS로 모두 추출",
        "drop_create": "아카이브를 생성하려면 여러 파일을 여기에 놓으세요",
        "creating": "아카이브 생성 중...",
        "selected_files": "선택된 파일",
        "btn_create_paf": ".paf 생성",
        "btn_create_zip": ".zip 생성 (OS 마운트 가능)",
        "empty_archive": "아카이브가 비어 있습니다.",
        "extract_success": "✅ Virtual FS (/out/)로 성공적으로 추출됨",
        "extract_fail": "❌ 추출 실패.",
        "alert_need_paf": ".paf 파일을 선택하세요.",
        "alert_parse_err": "아카이브 내용을 구문 분석하는 중 오류 발생.",
        "alert_read_err": "아카이브 내용을 읽지 못했습니다.",
        "alert_wasm_err": "WASM 모듈이 아직 로드되지 않았습니다.",
        "alert_paf_err": "PAF 아카이브 생성 실패.",
        "alert_zip_err": "ZIP 생성 중 오류 발생."
    },
    "it": {
        "title": "📦 Strumento Web PAF",
        "subtitle": "Formato di archivio parallelo ad alta velocità",
        "tab_viewer": "Visualizzatore (Viewer)",
        "tab_package": "Pacchetto (Package)",
        "drop_view": "Trascina qui un file .paf per visualizzarlo",
        "processing": "Elaborazione archivio...",
        "archive_contents": "Contenuto archivio",
        "th_path": "Percorso file",
        "th_size": "Dimensione (Byte)",
        "extract_btn": "Estrai tutto su Virtual FS",
        "drop_create": "Trascina qui più file per creare un archivio",
        "creating": "Creazione archivio in corso...",
        "selected_files": "File selezionati",
        "btn_create_paf": "Crea .paf",
        "btn_create_zip": "Crea .zip (Montabile su OS)",
        "empty_archive": "L'archivio è vuoto.",
        "extract_success": "✅ Estratto con successo su Virtual FS (/out/)",
        "extract_fail": "❌ Estrazione fallita.",
        "alert_need_paf": "Seleziona un file .paf.",
        "alert_parse_err": "Errore durante l'analisi del contenuto dell'archivio.",
        "alert_read_err": "Impossibile leggere il contenuto dell'archivio.",
        "alert_wasm_err": "Modulo WASM non ancora caricato.",
        "alert_paf_err": "Impossibile creare l'archivio PAF.",
        "alert_zip_err": "Errore durante la creazione dello ZIP."
    }
};

let currentLang = "en";

function initI18n() {
    // Detect browser language
    const browserLang = navigator.language.split('-')[0];
    if (translations[browserLang]) {
        currentLang = browserLang;
    }
    
    // Set select box value
    const langSelect = document.getElementById("lang-select");
    if (langSelect) {
        langSelect.value = currentLang;
        langSelect.addEventListener("change", (e) => {
            setLanguage(e.target.value);
        });
    }
    
    setLanguage(currentLang);
}

function setLanguage(langCode) {
    if (!translations[langCode]) return;
    currentLang = langCode;
    const dict = translations[langCode];
    
    // Replace all elements with data-i18n attribute
    document.querySelectorAll("[data-i18n]").forEach(el => {
        const key = el.getAttribute("data-i18n");
        if (dict[key]) {
            el.innerText = dict[key];
        }
    });
    
    // Update HTML lang attribute
    document.documentElement.lang = langCode;
}

// Function to get translation by key (useful for dynamic texts)
function t(key) {
    return translations[currentLang][key] || key;
}

// Global exposure
window.initI18n = initI18n;
window.t = t;
