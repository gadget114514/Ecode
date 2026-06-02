#include "PromptsLocalization.h"

PromptsLocalization::PromptsLocalization() {}

void PromptsLocalization::SetLanguage(const std::string &lang) {
    auto langs = SupportedLanguages();
    for (auto &l : langs) {
        if (l == lang) { m_currentLang = lang; return; }
    }
    m_currentLang = "en";
}

std::vector<std::string> PromptsLocalization::SupportedLanguages() {
    return {"en", "ja", "fr", "es", "pt", "de"};
}

std::wstring PromptsLocalization::GetString(const std::string &key) const {
    auto &translations = GetTranslations();
    auto itLang = translations.find(m_currentLang);
    if (itLang != translations.end()) {
        auto itStr = itLang->second.find(key);
        if (itStr != itLang->second.end())
            return itStr->second;
    }
    // Fallback to English
    auto itEn = translations.find("en");
    if (itEn != translations.end()) {
        auto itStr = itEn->second.find(key);
        if (itStr != itEn->second.end())
            return itStr->second;
    }
    return L"[" + std::wstring(key.begin(), key.end()) + L"]";
}

const std::map<std::string, std::map<std::string, std::wstring>> &PromptsLocalization::GetTranslations() const {
    static std::map<std::string, std::map<std::string, std::wstring>> t = {
        {"en", {
            {"AppName", L"Prompts"},
            {"New", L"New"},
            {"Open", L"Open"},
            {"Save", L"Save"},
            {"SaveAs", L"Save As"},
            {"Config", L"Config"},
            {"RunPipeline", L"Run Pipeline"},
            {"Search", L"Search"},
            {"Tree", L"Tree"},
            {"List", L"List"},
            {"Editor", L"Editor"},
            {"Messages", L"Messages"},
            {"AddChild", L"Add Child"},
            {"Update", L"Update"},
            {"Remove", L"Remove"},
            {"Copy", L"Copy"},
            {"Copied", L"Copied!"},
            {"Cancel", L"Cancel"},
            {"Pipeline", L"Pipeline"},
            {"History", L"History"},
            {"Settings", L"Settings"},
            {"Language", L"Language"},
        }},
        {"ja", {
            {"AppName", L"プロンプト"},
            {"New", L"新規"},
            {"Open", L"開く"},
            {"Save", L"保存"},
            {"SaveAs", L"名前を付けて保存"},
            {"Config", L"設定"},
            {"RunPipeline", L"パイプライン実行"},
            {"Search", L"検索"},
            {"Tree", L"ツリー"},
            {"List", L"リスト"},
            {"Editor", L"エディタ"},
            {"Messages", L"メッセージ"},
            {"AddChild", L"子ノード追加"},
            {"Update", L"更新"},
            {"Remove", L"削除"},
            {"Copy", L"コピー"},
            {"Copied", L"コピーしました！"},
            {"Cancel", L"キャンセル"},
            {"Pipeline", L"パイプライン"},
            {"History", L"履歴"},
            {"Settings", L"設定"},
            {"Language", L"言語"},
        }},
        {"fr", {
            {"AppName", L"Prompts"},
            {"New", L"Nouveau"},
            {"Open", L"Ouvrir"},
            {"Save", L"Enregistrer"},
            {"SaveAs", L"Enregistrer sous"},
            {"Config", L"Configuration"},
            {"RunPipeline", L"Exécuter"},
            {"Search", L"Rechercher"},
            {"Tree", L"Arbre"},
            {"List", L"Liste"},
            {"Editor", L"Éditeur"},
            {"Messages", L"Messages"},
            {"AddChild", L"Ajouter enfant"},
            {"Update", L"Mettre à jour"},
            {"Remove", L"Supprimer"},
            {"Copy", L"Copier"},
            {"Copied", L"Copié !"},
            {"Cancel", L"Annuler"},
            {"Pipeline", L"Pipeline"},
            {"History", L"Historique"},
            {"Settings", L"Paramètres"},
            {"Language", L"Langue"},
        }},
        {"es", {
            {"AppName", L"Prompts"},
            {"New", L"Nuevo"},
            {"Open", L"Abrir"},
            {"Save", L"Guardar"},
            {"SaveAs", L"Guardar como"},
            {"Config", L"Configurar"},
            {"RunPipeline", L"Ejecutar"},
            {"Search", L"Buscar"},
            {"Tree", L"Árbol"},
            {"List", L"Lista"},
            {"Editor", L"Editor"},
            {"Messages", L"Mensajes"},
            {"AddChild", L"Añadir hijo"},
            {"Update", L"Actualizar"},
            {"Remove", L"Eliminar"},
            {"Copy", L"Copiar"},
            {"Copied", L"¡Copiado!"},
            {"Cancel", L"Cancelar"},
            {"Pipeline", L"Pipeline"},
            {"History", L"Historial"},
            {"Settings", L"Ajustes"},
            {"Language", L"Idioma"},
        }},
        {"pt", {
            {"AppName", L"Prompts"},
            {"New", L"Novo"},
            {"Open", L"Abrir"},
            {"Save", L"Salvar"},
            {"SaveAs", L"Salvar como"},
            {"Config", L"Configurar"},
            {"RunPipeline", L"Executar"},
            {"Search", L"Pesquisar"},
            {"Tree", L"Árvore"},
            {"List", L"Lista"},
            {"Editor", L"Editor"},
            {"Messages", L"Mensagens"},
            {"AddChild", L"Adicionar filho"},
            {"Update", L"Atualizar"},
            {"Remove", L"Remover"},
            {"Copy", L"Copiar"},
            {"Copied", L"Copiado!"},
            {"Cancel", L"Cancelar"},
            {"Pipeline", L"Pipeline"},
            {"History", L"Histórico"},
            {"Settings", L"Configurações"},
            {"Language", L"Idioma"},
        }},
        {"de", {
            {"AppName", L"Prompts"},
            {"New", L"Neu"},
            {"Open", L"Öffnen"},
            {"Save", L"Speichern"},
            {"SaveAs", L"Speichern unter"},
            {"Config", L"Konfiguration"},
            {"RunPipeline", L"Ausführen"},
            {"Search", L"Suchen"},
            {"Tree", L"Baum"},
            {"List", L"Liste"},
            {"Editor", L"Editor"},
            {"Messages", L"Nachrichten"},
            {"AddChild", L"Kind hinzufügen"},
            {"Update", L"Aktualisieren"},
            {"Remove", L"Entfernen"},
            {"Copy", L"Kopieren"},
            {"Copied", L"Kopiert!"},
            {"Cancel", L"Abbrechen"},
            {"Pipeline", L"Pipeline"},
            {"History", L"Verlauf"},
            {"Settings", L"Einstellungen"},
            {"Language", L"Sprache"},
        }},
    };
    return t;
}
