#pragma once

#include <array>
#include <string_view>

namespace tetrisphere::launcher {

inline constexpr std::array language_codes{"en", "es", "de", "fr", "it", "pt"};
inline constexpr std::array language_names{"English", "Español", "Deutsch", "Français", "Italiano", "Português"};

enum class Text {
    Subtitle, RomTitle, RomHelp, SelectRom, Compatible, Change, Continue, Back,
    Destination, Browse, Install, Installing, Copying, Preparing, Verifying,
    Installed, Play, AddSteam, Exit, Account, AccountHelp, Artwork, Cover, Wide,
    Hero, Logo, Icon, KeepArt, LocalArt, SteamNotice, Add, Added, Done, Retry,
    Select, RomRead, RomInvalid, RomUnsupported, DestinationError, InstallError,
    PrepareError, PreferenceError, PlayError, NoSteam, RunningGame, SteamClose,
    SteamWrite, SteamReopen, ArtError, Update, Download, Later,
    ArtLoading, ArtUnavailable, ArtDownloadFailed, Count
};

inline constexpr const char* strings[][6] = {
    {"A Tetrisphere recomp", "Una recompilación de Tetrisphere", "Eine Tetrisphere-Rekompilierung", "Une recompilation de Tetrisphere", "Una ricompilazione di Tetrisphere", "Uma recompilação de Tetrisphere"},
    {"Select your game file", "Selecciona el archivo del juego", "Spieldatei auswählen", "Sélectionner le fichier du jeu", "Seleziona il file di gioco", "Selecione o arquivo do jogo"},
    {"Choose a Tetrisphere US NTPE revision 0 ROM or ZIP.", "Elige una ROM o ZIP de Tetrisphere US NTPE revisión 0.", "Wähle eine ROM oder ZIP von Tetrisphere US NTPE Revision 0.", "Choisissez une ROM ou un ZIP Tetrisphere US NTPE révision 0.", "Scegli una ROM o uno ZIP di Tetrisphere US NTPE revisione 0.", "Escolha uma ROM ou um ZIP de Tetrisphere US NTPE revisão 0."},
    {"Select ROM", "Seleccionar ROM", "ROM auswählen", "Sélectionner la ROM", "Seleziona ROM", "Selecionar ROM"},
    {"Compatible ROM", "ROM compatible", "Kompatible ROM", "ROM compatible", "ROM compatibile", "ROM compatível"},
    {"Change", "Cambiar", "Ändern", "Changer", "Cambia", "Alterar"},
    {"Continue", "Continuar", "Weiter", "Continuer", "Continua", "Continuar"},
    {"Back", "Atrás", "Zurück", "Retour", "Indietro", "Voltar"},
    {"Choose install location", "Elige dónde instalar", "Installationsort wählen", "Choisir le dossier d’installation", "Scegli dove installare", "Escolha o local da instalação"},
    {"Browse…", "Examinar…", "Durchsuchen…", "Parcourir…", "Sfoglia…", "Procurar…"},
    {"Install", "Instalar", "Installieren", "Installer", "Installa", "Instalar"},
    {"Installing Blocksphere", "Instalando Blocksphere", "Blocksphere wird installiert", "Installation de Blocksphere", "Installazione di Blocksphere", "Instalando Blocksphere"},
    {"Copying game files…", "Copiando archivos del juego…", "Spieldateien werden kopiert…", "Copie des fichiers du jeu…", "Copia dei file di gioco…", "Copiando os arquivos do jogo…"},
    {"Preparing this PC…", "Preparando este equipo…", "Dieser PC wird vorbereitet…", "Préparation de cet ordinateur…", "Preparazione del PC…", "Preparando este computador…"},
    {"Verifying installation…", "Verificando la instalación…", "Installation wird geprüft…", "Vérification de l’installation…", "Verifica dell’installazione…", "Verificando a instalação…"},
    {"Installed", "Instalado", "Installiert", "Installé", "Installato", "Instalado"},
    {"Play", "Jugar", "Spielen", "Jouer", "Gioca", "Jogar"},
    {"Add to Steam", "Añadir a Steam", "Zu Steam hinzufügen", "Ajouter à Steam", "Aggiungi a Steam", "Adicionar à Steam"},
    {"Exit launcher", "Cerrar lanzador", "Launcher beenden", "Fermer le lanceur", "Chiudi il launcher", "Fechar iniciador"},
    {"Choose a Steam account", "Elige una cuenta de Steam", "Steam-Konto auswählen", "Choisir un compte Steam", "Scegli un account Steam", "Escolha uma conta Steam"},
    {"Accounts on this computer", "Cuentas de este equipo", "Konten auf diesem PC", "Comptes sur cet ordinateur", "Account su questo computer", "Contas deste computador"},
    {"Choose artwork", "Elige las imágenes", "Grafiken auswählen", "Choisir les visuels", "Scegli le immagini", "Escolha as imagens"},
    {"Cover", "Portada", "Cover", "Jaquette", "Copertina", "Capa"},
    {"Wide cover", "Portada horizontal", "Breites Cover", "Jaquette horizontale", "Copertina orizzontale", "Capa horizontal"},
    {"Hero", "Fondo", "Hintergrund", "Arrière-plan", "Sfondo", "Plano de fundo"},
    {"Logo", "Logotipo", "Logo", "Logo", "Logo", "Logotipo"},
    {"Icon", "Icono", "Symbol", "Icône", "Icona", "Ícone"},
    {"Keep current", "Conservar la actual", "Aktuelle behalten", "Garder le visuel actuel", "Mantieni l’immagine attuale", "Manter a atual"},
    {"Choose image…", "Elegir imagen…", "Bild auswählen…", "Choisir une image…", "Scegli immagine…", "Escolher imagem…"},
    {"Save any running game. If Steam is open, Add will close it and try to reopen it afterward.", "Guarda cualquier partida. Si Steam está abierto, se cerrará y se intentará volver a abrir después de añadir el juego.", "Speichere laufende Spiele. Falls Steam geöffnet ist, wird es geschlossen und danach erneut gestartet, sofern möglich.", "Sauvegardez vos parties. Si Steam est ouvert, il sera fermé puis relancé si possible.", "Salva le partite in corso. Se Steam è aperto, verrà chiuso e riaperto se possibile.", "Salve as partidas em andamento. Se a Steam estiver aberta, ela será fechada e reaberta se possível."},
    {"Add", "Añadir", "Hinzufügen", "Ajouter", "Aggiungi", "Adicionar"},
    {"Added to Steam", "Añadido a Steam", "Zu Steam hinzugefügt", "Ajouté à Steam", "Aggiunto a Steam", "Adicionado à Steam"},
    {"Done", "Listo", "Fertig", "Terminé", "Fine", "Concluído"},
    {"Retry", "Reintentar", "Erneut versuchen", "Réessayer", "Riprova", "Tentar novamente"},
    {"Select", "Seleccionar", "Auswählen", "Sélectionner", "Seleziona", "Selecionar"},
    {"Cannot read the file. Choose a readable ROM or ZIP.", "No se puede leer el archivo. Elige una ROM o un ZIP accesible.", "Datei nicht lesbar. Wähle eine lesbare ROM oder ZIP.", "Fichier illisible. Choisissez une ROM ou un ZIP lisible.", "File illeggibile. Scegli una ROM o uno ZIP leggibile.", "Não foi possível ler o arquivo. Escolha uma ROM ou ZIP legível."},
    {"This file is incomplete or damaged. Choose a complete ROM or ZIP.", "El archivo está incompleto o dañado. Elige una ROM o un ZIP completo.", "Die Datei ist unvollständig oder beschädigt. Wähle eine vollständige ROM oder ZIP.", "Ce fichier est incomplet ou endommagé. Choisissez une ROM ou un ZIP complet.", "Il file è incompleto o danneggiato. Scegli una ROM o uno ZIP completo.", "O arquivo está incompleto ou corrompido. Escolha uma ROM ou ZIP completo."},
    {"Incompatible game file. Choose Tetrisphere US NTPE revision 0.", "Archivo incompatible. Elige Tetrisphere US NTPE revisión 0.", "Inkompatible Datei. Wähle Tetrisphere US NTPE Revision 0.", "Fichier incompatible. Choisissez Tetrisphere US NTPE révision 0.", "File incompatibile. Scegli Tetrisphere US NTPE revisione 0.", "Arquivo incompatível. Escolha Tetrisphere US NTPE revisão 0."},
    {"Choose an empty, writable installation folder.", "Elige una carpeta vacía con permiso de escritura.", "Wähle einen leeren, beschreibbaren Ordner.", "Choisissez un dossier vide et accessible en écriture.", "Scegli una cartella vuota e scrivibile.", "Escolha uma pasta vazia com permissão de escrita."},
    {"Installation failed. Check the package and available space, then retry.", "Falló la instalación. Revisa el paquete y el espacio disponible y reintenta.", "Installation fehlgeschlagen. Prüfe das Paket und den freien Speicherplatz.", "Échec de l’installation. Vérifiez le paquet et l’espace libre, puis réessayez.", "Installazione non riuscita. Controlla il pacchetto e lo spazio libero.", "Falha na instalação. Verifique o pacote e o espaço disponível."},
    {"Preparation failed. Check the game file and retry.", "Falló la preparación. Revisa el archivo del juego y reintenta.", "Vorbereitung fehlgeschlagen. Prüfe die Spieldatei und versuche es erneut.", "Échec de la préparation. Vérifiez le fichier du jeu et réessayez.", "Preparazione non riuscita. Controlla il file di gioco e riprova.", "Falha na preparação. Verifique o arquivo do jogo e tente novamente."},
    {"Could not save preferences. Check folder permissions.", "No se pudieron guardar las preferencias. Revisa los permisos.", "Einstellungen konnten nicht gespeichert werden. Prüfe die Zugriffsrechte.", "Impossible d’enregistrer les préférences. Vérifiez les autorisations.", "Impossibile salvare le preferenze. Controlla i permessi.", "Não foi possível salvar as preferências. Verifique as permissões."},
    {"Could not start Blocksphere. Check the installation.", "No se pudo iniciar Blocksphere. Revisa la instalación.", "Blocksphere konnte nicht gestartet werden. Prüfe die Installation.", "Impossible de lancer Blocksphere. Vérifiez l’installation.", "Impossibile avviare Blocksphere. Controlla l’installazione.", "Não foi possível iniciar Blocksphere. Verifique a instalação."},
    {"No supported Steam account was found. Sign in to Steam and try again.", "No se encontró una cuenta de Steam compatible. Inicia sesión en Steam y reintenta.", "Kein unterstütztes Steam-Konto gefunden. Melde dich bei Steam an und versuche es erneut.", "Aucun compte Steam compatible trouvé. Connectez-vous à Steam et réessayez.", "Nessun account Steam compatibile. Accedi a Steam e riprova.", "Nenhuma conta Steam compatível encontrada. Entre na Steam e tente novamente."},
    {"A game is running. Save and close it before adding the shortcut.", "Hay un juego abierto. Guarda y ciérralo antes de añadir el acceso.", "Ein Spiel läuft. Speichere und beende es, bevor du die Verknüpfung hinzufügst.", "Un jeu est en cours. Sauvegardez et fermez-le avant d’ajouter le raccourci.", "Un gioco è aperto. Salva e chiudilo prima di aggiungere il collegamento.", "Há um jogo aberto. Salve e feche-o antes de adicionar o atalho."},
    {"Steam did not close. Close it normally, then retry.", "Steam no se cerró. Ciérralo normalmente y reintenta.", "Steam wurde nicht beendet. Beende es regulär und versuche es erneut.", "Steam ne s’est pas fermé. Fermez-le normalement et réessayez.", "Steam non si è chiuso. Chiudilo normalmente e riprova.", "A Steam não foi fechada. Feche-a normalmente e tente novamente."},
    {"Could not save the Steam shortcut. Check access to the Steam folder.", "No se pudo guardar el acceso. Revisa los permisos de la carpeta de Steam.", "Steam-Verknüpfung konnte nicht gespeichert werden. Prüfe den Ordnerzugriff.", "Impossible d’enregistrer le raccourci. Vérifiez l’accès au dossier Steam.", "Impossibile salvare il collegamento. Controlla l’accesso alla cartella Steam.", "Não foi possível salvar o atalho. Verifique o acesso à pasta da Steam."},
    {"The shortcut was saved. Open Steam manually.", "Se guardó el acceso. Abre Steam manualmente.", "Die Verknüpfung wurde gespeichert. Öffne Steam manuell.", "Le raccourci a été enregistré. Ouvrez Steam manuellement.", "Collegamento salvato. Apri Steam manualmente.", "O atalho foi salvo. Abra a Steam manualmente."},
    {"Choose a readable PNG or JPEG image.", "Elige una imagen PNG o JPEG accesible.", "Wähle ein lesbares PNG- oder JPEG-Bild.", "Choisissez une image PNG ou JPEG lisible.", "Scegli un’immagine PNG o JPEG leggibile.", "Escolha uma imagem PNG ou JPEG legível."},
    {"Update available", "Actualización disponible", "Update verfügbar", "Mise à jour disponible", "Aggiornamento disponibile", "Atualização disponível"},
    {"Download update", "Descargar actualización", "Update herunterladen", "Télécharger la mise à jour", "Scarica aggiornamento", "Baixar atualização"},
    {"Later", "Más tarde", "Später", "Plus tard", "Più tardi", "Mais tarde"},
    {"Loading SteamGridDB artwork…", "Cargando imágenes de SteamGridDB…", "SteamGridDB-Grafiken werden geladen…", "Chargement des visuels SteamGridDB…", "Caricamento delle immagini SteamGridDB…", "Carregando imagens do SteamGridDB…"},
    {"SteamGridDB artwork is unavailable for this category.", "No hay imágenes de SteamGridDB disponibles en esta categoría.", "SteamGridDB-Grafiken sind in dieser Kategorie nicht verfügbar.", "Aucun visuel SteamGridDB disponible dans cette catégorie.", "Immagini SteamGridDB non disponibili per questa categoria.", "Imagens do SteamGridDB indisponíveis nesta categoria."},
    {"Could not download the selected image. Retry or keep the current artwork.", "No se pudo descargar la imagen elegida. Reintenta o conserva la actual.", "Bild konnte nicht geladen werden. Erneut versuchen oder aktuelles Bild behalten.", "Impossible de télécharger l’image choisie. Réessayez ou gardez le visuel actuel.", "Impossibile scaricare l’immagine selezionata. Riprova o mantieni quella attuale.", "Não foi possível baixar a imagem. Tente novamente ou mantenha a atual."},
};
static_assert(std::size(strings) == static_cast<unsigned>(Text::Count));

inline const char* translate(Text key, unsigned language) {
    return strings[static_cast<unsigned>(key)][language < language_codes.size() ? language : 0];
}

} // namespace tetrisphere::launcher
