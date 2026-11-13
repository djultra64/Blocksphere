#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <utility>

namespace tetrisphere {

enum class MenuLanguage { English, Spanish, German, French, Italian, Portuguese };

inline constexpr std::array menu_languages{
    std::pair{"en", MenuLanguage::English}, std::pair{"es", MenuLanguage::Spanish},
    std::pair{"de", MenuLanguage::German}, std::pair{"fr", MenuLanguage::French},
    std::pair{"it", MenuLanguage::Italian}, std::pair{"pt", MenuLanguage::Portuguese},
};

inline std::optional<MenuLanguage> parse_menu_language(std::string_view code) {
    if (code.starts_with("\xEF\xBB\xBF")) code.remove_prefix(3);
    const auto first = code.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    code = code.substr(first, code.find_last_not_of(" \t\r\n") - first + 1);
    for (const auto& [name, language] : menu_languages) {
        if (code == name) return language;
    }
    return {};
}

inline MenuLanguage read_menu_language(const std::filesystem::path& directory,
                                       std::string_view override_code = {}) {
    if (const auto language = parse_menu_language(override_code)) return *language;
    std::ifstream file(directory / "language.txt", std::ios::binary);
    std::array<char, 32> buffer{};
    file.read(buffer.data(), buffer.size());
    if (!file.eof() || file.bad()) return MenuLanguage::English;
    return parse_menu_language({buffer.data(), static_cast<std::size_t>(file.gcount())})
        .value_or(MenuLanguage::English);
}

inline std::atomic<MenuLanguage> current_menu_language{MenuLanguage::English};

inline void set_menu_language(MenuLanguage language) {
    current_menu_language.store(language, std::memory_order_relaxed);
}

inline MenuLanguage menu_language() {
    return current_menu_language.load(std::memory_order_relaxed);
}

inline void initialize_menu_language(const std::filesystem::path& directory) {
    const char* override_code = std::getenv("TETRISPHERE_LANGUAGE");
    set_menu_language(read_menu_language(directory, override_code ? override_code : ""));
}

// The original menu atlas contains uppercase ASCII. Use short transliterations
// for unavailable accents and preserve physical button names outside this table.
inline constexpr std::string_view menu_translations[][6]{
    // English, Spanish, German, French, Italian, Portuguese (Brazil).
    {"SINGLE", "SOLO", "EINZEL", "SOLO", "SINGOLO", "SOLO"},
    {"VS", "VS", "VS", "VS", "VS", "VS"},
    {"PRACTICE", "PRACTICA", "UEBUNG", "PRATIQUE", "PRATICA", "PRATICA"},
    {"OPTIONS", "OPCIONES", "OPTIONEN", "OPTIONS", "OPZIONI", "OPCOES"},
    {"TRAINING", "TUTORIAL", "TRAINING", "INITIATION", "TUTORIAL", "TREINO"},
    {"CLOSE GAME", "SALIR", "BEENDEN", "QUITTER", "ESCI", "SAIR"},
    {"RESCUE", "RESCATE", "RETTUNG", "SAUVETAGE", "SOCCORSO", "RESGATE"},
    {"HIDE&SEEK", "ESCONDITE", "VERSTECK", "CACHECACHE", "NASCONDINO", "ESCONDE"},
    {"PUZZLE", "PUZLE", "PUZZLE", "PUZZLE", "PUZZLE", "PUZZLE"},
    {"TIME TRIAL", "A TIEMPO", "ZEITRENNEN", "CHRONO", "A TEMPO", "TEMPO"},
    {"VS CPU", "VS CPU", "VS CPU", "VS CPU", "VS CPU", "VS CPU"},
    {"LINES", "LINEAS", "LINIEN", "LIGNES", "LINEE", "LINHAS"},
    {"AUDIO", "AUDIO", "AUDIO", "AUDIO", "AUDIO", "AUDIO"},
    {"DATA EDIT", "DATOS", "DATEN", "DONNEES", "DATI", "DADOS"},
    {"SCORES", "PUNTAJES", "REKORDE", "SCORES", "PUNTEGGI", "PLACARES"},
    {"CONTROLS", "CONTROLES", "STEUERUNG", "COMMANDES", "COMANDI", "CONTROLES"},
    {"GRAPHICS", "GRAFICOS", "GRAFIK", "GRAPHISMES", "GRAFICA", "GRAFICOS"},
    {"BASICS", "BASICO", "GRUNDLAGEN", "BASES", "BASI", "BASICO"},
    {"ADVANCED", "AVANZADO", "EXPERTEN", "AVANCE", "AVANZATO", "AVANCADO"},
    {"BATTLE", "BATALLA", "KAMPF", "BATAILLE", "BATTAGLIA", "BATALHA"},
    {"RULES", "REGLAS", "REGELN", "REGLES", "REGOLE", "REGRAS"},
    {"CONTINUE", "SEGUIR", "WEITER", "CONTINUER", "CONTINUA", "CONTINUAR"},
    {"EXIT", "SALIR", "ENDE", "QUITTER", "ESCI", "SAIR"},
    {"START GAME", "JUGAR", "SPIELSTART", "JOUER", "GIOCA", "JOGAR"},
    {"LEVEL", "NIVEL", "STUFE", "NIVEAU", "LIVELLO", "NIVEL"},
    {"NAME", "NOMBRE", "NAME", "NOM", "NOME", "NOME"},
    {"BOT", "CPU", "CPU", "CPU", "CPU", "CPU"},
    {"GOAL", "OBJETIVO", "ZIEL", "OBJECTIF", "OBIETTIVO", "OBJETIVO"},
    {"TRY AGAIN", "REINTENTAR", "NOCHMAL", "REESSAYER", "RIPROVA", "TENTE NOVO"},
    {"LOAD GAME", "CARGAR", "LADEN", "CHARGER", "CARICA", "CARREGAR"},
    {"DELETE", "BORRAR", "LOESCHEN", "EFFACER", "ELIMINA", "APAGAR"},
    {"NO", "NO", "NEIN", "NON", "NO", "NAO"},
    {"YES", "SI", "JA", "OUI", "SI", "SIM"},
    {"RANDOM", "AZAR", "ZUFALL", "ALEATOIRE", "CASUALE", "ALEATORIO"},
    {"CHOOSE", "ELEGIR", "WAHL", "CHOISIR", "SCEGLI", "ESCOLHER"},
    {"WATCH AGAIN", "REPETIR", "NOCHMAL", "REVOIR", "RIVEDI", "REVER"},
    {"CREDITS", "CREDITOS", "MITWIRKEN", "CREDITS", "CREDITI", "CREDITOS"},

    {"RESOLUTION", "RESOLUCION", "AUFLOESUNG", "RESOLUTION", "RISOLUZIONE", "RESOLUCAO"},
    {"ASPECT", "FORMATO", "BILDFORMAT", "FORMAT", "FORMATO", "FORMATO"},
    {"FRAME RATE", "TASA DE FPS", "BILDRATE", "CADENCE", "FREQUENZA", "TAXA DE FPS"},
    {"WINDOW", "VENTANA", "FENSTER", "FENETRE", "FINESTRA", "JANELA"},
    {"PRESENT FILTER", "FILTRO DE SALIDA", "AUSGABEFILTER", "FILTRE IMAGE", "FILTRO USCITA", "FILTRO DE SAIDA"},
    {"TEXTURE FILTER", "FILTRO TEXTURAS", "TEXTURFILTER", "FILTRE TEXTURE", "FILTRO TEXTURE", "FILTRO TEXTURA"},
    {"ORIGINAL", "ORIGINAL", "ORIGINAL", "ORIGINAL", "ORIGINALE", "ORIGINAL"},
    {"UNLIMITED", "SIN LIMITE", "UNBEGRENZT", "ILLIMITE", "ILLIMITATO", "SEM LIMITE"},
    {"WINDOWED", "EN VENTANA", "FENSTER", "FENETRE", "IN FINESTRA", "EM JANELA"},
    {"FULLSCREEN", "PANTALLA COMPLETA", "VOLLBILD", "PLEIN ECRAN", "SCHERMO INTERO", "TELA CHEIA"},
    {"OFF", "NO", "AUS", "NON", "NO", "NAO"},
    {"ON", "SI", "AN", "OUI", "SI", "SIM"},
    {"NEAREST", "CERCANO", "NAECHSTER", "PLUS PROCHE", "PIU VICINO", "MAIS PROXIMO"},
    {"LINEAR", "LINEAL", "LINEAR", "LINEAIRE", "LINEARE", "LINEAR"},
    {"PIXEL", "PIXEL", "PIXEL", "PIXEL", "PIXEL", "PIXEL"},
    {"3-POINT", "3 PUNTOS", "3 PUNKTE", "3 POINTS", "3 PUNTI", "3 PONTOS"},
    {"BACK", "ATRAS", "ZURUECK", "RETOUR", "INDIETRO", "VOLTAR"},
    {"APPLIES AFTER RESTART", "SE APLICA AL REINICIAR", "WIRKT NACH NEUSTART", "APRES REDEMARRAGE", "DOPO IL RIAVVIO", "APOS REINICIAR"},
    {"APPLIES NOW", "SE APLICA AHORA", "WIRKT SOFORT", "EFFET IMMEDIAT", "EFFETTO IMMEDIATO", "EFEITO IMEDIATO"},
    {"LEFT RIGHT CHANGE", "IZQ DER CAMBIAR", "LINKS RECHTS AENDERN", "GAUCHE DROITE MODIF", "SINISTRA DESTRA", "ESQ DIR ALTERAR"},

    {"LAYOUT", "TIPO", "LAYOUT", "TYPE", "TIPO", "TIPO"},
    {"KEYS", "TECLAS", "TASTEN", "CLAVIER", "TASTI", "TECLAS"},
    {"AUTO", "AUTO", "AUTO", "AUTO", "AUTO", "AUTO"},
    {"DROP SELECT", "SOLTAR ELEGIR", "SETZEN WAHL", "POSER CHOISIR", "POSA SCEGLI", "SOLTAR ESCOLHER"},
    {"SLIDE BACK", "MOVER ATRAS", "SCHIEBEN ZUR", "GLISSER RETOUR", "SCORRI INDIETRO", "DESLIZE VOLTAR"},
    {"RESTART PUZZLE", "REHACER PUZLE", "PUZZLE NEU", "REFAIRE PUZZLE", "RIFAI PUZZLE", "REFAZER PUZZLE"},
    {"MAGIC", "MAGIA", "MAGIE", "MAGIE", "MAGIA", "MAGIA"},
    {"ZOOM IN", "ACERCAR", "ZOOM +", "ZOOM +", "ZOOM +", "ZOOM +"},
    {"ZOOM OUT", "ALEJAR", "ZOOM -", "ZOOM -", "ZOOM -", "ZOOM -"},
    {"MOVE", "MOVER", "BEWEGEN", "BOUGER", "MUOVI", "MOVER"},
    {"ARROWS", "FLECHAS", "PFEILE", "FLECHES", "FRECCE", "SETAS"},
    {"PAUSE", "PAUSA", "PAUSE", "PAUSE", "PAUSA", "PAUSA"},
    {"DEFAULTS", "PREDETER", "STANDARD", "DEFAUTS", "PREDEFIN", "PADRAO"},
    {"DISCARD", "DESCARTAR", "VERWERFEN", "ANNULER", "SCARTA", "DESCARTAR"},
    {"APPLY", "APLICAR", "ANWENDEN", "APPLIQUER", "APPLICA", "APLICAR"},
    {"KEEP EDITING", "SEGUIR EDITANDO", "WEITER BEARB", "CONTINUER", "CONTINUA", "CONTINUAR"},
    {"SAVING", "GUARDANDO", "SPEICHERN", "SAUVE EN COURS", "SALVATAGGIO", "SALVANDO"},
    {"PRESS BUTTON", "PULSA UN BOTON", "TASTE DRUECKEN", "APPUIE SUR BOUTON", "PREMI UN TASTO", "APERTE UM BOTAO"},
    {"SHARED BUTTON", "BOTON COMPARTIDO", "GETEILTE TASTE", "BOUTON PARTAGE", "TASTO CONDIVISO", "BOTAO DUPLO"},
    {"PUZZLE ONLY", "SOLO PUZLE", "NUR PUZZLE", "PUZZLE SEUL", "SOLO PUZZLE", "SO PUZZLE"},
    {"CHANGES NOT APPLIED", "CAMBIOS SIN APLICAR", "NICHT UEBERNOMMEN", "MODIFS NON APPLIQUEES", "MODIFICHE IN ATTESA", "MUDANCAS PENDENTES"},
    {"SELECT ACTION", "ELIGE ACCION", "AKTION WAEHLEN", "CHOISIS ACTION", "SCEGLI AZIONE", "ESCOLHA ACAO"},
    {"SELECT", "ELEGIR", "WAHL", "CHOISIR", "SCEGLI", "ESCOLHER"},
    {"SELECT AND BACK MUST DIFFER", "ELEGIR Y ATRAS DISTINTOS", "WAHL UND ZURUECK TRENNEN", "CHOIX ET RETOUR DISTINCTS", "SCEGLI E INDIETRO DIVERSI", "ESCOLHER E VOLTAR DISTINTOS"},
    {"RESTART AND MAGIC MUST DIFFER", "REINICIO Y MAGIA DISTINTOS", "NEU UND MAGIE TRENNEN", "REFAIRE ET MAGIE DISTINCTS", "RIFAI E MAGIA DIVERSI", "REINICIO E MAGIA DISTINTOS"},
    {"APPLY OR DISCARD TO LEAVE", "APLICA O DESCARTA", "ANWENDEN ODER VERWERFEN", "APPLIQUE OU ANNULE", "APPLICA O SCARTA", "APLIQUE OU DESCARTE"},
    {"USE A FACE BUTTON", "USA BOTON FRONTAL", "FRONTTASTE NUTZEN", "UTILISE UN BOUTON", "USA TASTO FRONTALE", "USE BOTAO FRONTAL"},
    {"APPLY THEN RESTART", "APLICA Y REINICIA", "ANWENDEN UND NEUSTART", "APPLIQUE ET REDEMARRE", "APPLICA E RIAVVIA", "APLIQUE E REINICIE"},
    {"SAVED RESTART REQUIRED", "GUARDADO REINICIA", "GESPEICHERT NEUSTART NOETIG", "SAUVEGARDE REDEMARRE", "SALVATO RIAVVIA", "SALVO REINICIE"},
    {"SAVED", "GUARDADO", "GESPEICHERT", "SAUVEGARDE", "SALVATO", "SALVO"},
    {"SAVE FAILED TRY AGAIN", "ERROR AL GUARDAR", "SPEICHERFEHLER", "ECHEC SAUVEGARDE", "ERRORE SALVATAGGIO", "ERRO AO SALVAR"},
};

inline std::string_view menu_translation(std::string_view english,
                                         MenuLanguage language = menu_language()) {
    const auto index = static_cast<std::size_t>(language);
    if (index == 0 || index >= menu_languages.size()) return english;
    for (const auto& row : menu_translations) {
        if (row[0] == english) return row[index];
    }
    return english;
}

// Original static NTPE rev0 labels used by the measured submenu rows in
// func_8004C7E8. Leave other strings, including user-entered names, alone.
inline std::string_view native_menu_translation(std::uint32_t address,
                                               MenuLanguage language = menu_language()) {
    if (language == MenuLanguage::English) return {};
    constexpr std::pair<std::uint32_t, std::string_view> labels[]{
        {0x800EC9A0u, "RESCUE"}, {0x800EC9A8u, "HIDE&SEEK"}, {0x800EC9B4u, "PUZZLE"},
        {0x800EC9BCu, "TIME TRIAL"}, {0x800EC9C8u, "VS CPU"}, {0x800EC9D0u, "LINES"},
        {0x800EC9D8u, "AUDIO"}, {0x800EC9E0u, "DATA EDIT"}, {0x800EC9ECu, "SCORES"},
        {0x800EC9F4u, "CONTROLS"}, {0x800ECA00u, "START GAME"}, {0x800ECA0Cu, "LEVEL"},
        {0x800ECA14u, "NAME"}, {0x800ECA1Cu, "BOT"}, {0x800ECA20u, "CONTINUE"},
        {0x800ECA2Cu, "GOAL"}, {0x800ECA34u, "AUDIO"}, {0x800ECA3Cu, "EXIT"},
        {0x800ECA44u, "TRY AGAIN"}, {0x800ECA50u, "EXIT"}, {0x800ECB10u, "AUTO"},
        {0x800ECB18u, "RANDOM"}, {0x800ECB20u, "CHOOSE"}, {0x800ECB28u, "CONTINUE"},
        {0x800ECB34u, "EXIT"}, {0x800ECB3Cu, "CONTINUE"}, {0x800ECB48u, "WATCH AGAIN"},
        {0x800ECB54u, "EXIT"}, {0x800ECB5Cu, "LOAD GAME"}, {0x800ECB68u, "DELETE"},
        {0x800ECB70u, "NO"}, {0x800ECB74u, "YES"}, {0x800ECB78u, "CONTINUE"},
        {0x800ECB84u, "DATA EDIT"}, {0x800ECB90u, "BASICS"}, {0x800ECB98u, "ADVANCED"},
        {0x800ECBA4u, "PUZZLE"}, {0x800ECBACu, "BATTLE"}, {0x800ECBB4u, "RULES"},
        {0x800ECBBCu, "LINES"}, {0x800ECBCCu, "LINES"}, {0x800ECBD4u, "CREDITS"},
    };
    for (const auto& [source, key] : labels) {
        if (source == address) return menu_translation(key, language);
    }
    return {};
}

} // namespace tetrisphere
