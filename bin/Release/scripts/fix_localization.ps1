$path = 'd:\ws\Ecode\src\Localization.cpp'
$content = [System.IO.File]::ReadAllText($path)

$replacements = @{
    "日本誁E" = "日本語";
    "lCeBuWin32 eLXgGfB^" = "ネイティブ Win32 テキストエディタ";
    "t@C(&F)" = "ファイル(&F)";
    "VK쐬(&N)" = "新規作成(&N)";
    "J(&O)..." = "開く(&O)...";
    "ۑ(&S)" = "保存(&S)";
    "Otĕۑ(&A)..." = "名前を付けて保存(&A)...";
    "‚(&C)" = "閉じる(&C)";
    "VXNb`obt@(&S)" = "新スクラッチバッファ(&S)";
    "(履歴なぁE" = "(履歴なし)";
    "I(&X)" = "終了(&X)";
    "ҏW(&E)" = "編集(&E)";
    "ɖ߂(&U)" = "元に戻す(&U)";
    "蒼(&R)" = "やり直し(&R)";
    "؂(&T)" = "切り取り(&T)";
    "Rs[(&C)" = "コピー(&C)";
    "\t(&P)" = "貼り付け(&P)";
    "ׂđI(&A)" = "すべて選択(&A)";
    "(&F)..." = "検索(&F)...";
    "u(&R)..." = "置換(&R)...";
    "wsֈړ(&G)..." = "指定行へ移動(&G)...";
    "\(&V)" = "表示(&V)";
    "UIvf̐؂ւ" = "UI要素の切り替え";
    "g(&I)" = "拡大(&I)";
    "k(&O)" = "縮小(&O)";
    "Y[Zbg(&R)" = "ズームリセット(&R)";
    "ݒ(&C)" = "設定(&C)";
    "ݒ(&S)" = "設定(&S)";
    "e[}I(&T)" = "テーマ選択(&T)";
    "ecodeinit.jsҏW(&E)" = "ecodeinit.jsを編集(&E)";
    "c[(&T)" = "ツール(&T)";
    "}Ns(&R)" = "マクロ実行(&R)";
    "XNvgR\[(&C)" = "スクリプトコンソール(&C)";
    "}NM[(&G)" = "マクロギャラリー(&G)";
    "obt@(&B)" = "バッファ(&B)";
    "wv(&H)" = "ヘルプ(&H)";
    "hLg(&D)" = "ドキュメント(&D)";
    "o[W(&A)" = "バージョン情報(&A)";
    "" = " "
}

foreach ($key in $replacements.Keys) {
    if ($content.Contains($key)) {
        $content = $content.Replace($key, $replacements[$key])
    }
}

[System.IO.File]::WriteAllText($path, $content, [System.Text.Encoding]::UTF8)
