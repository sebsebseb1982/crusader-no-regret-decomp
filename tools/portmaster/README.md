# Portage PortMaster (consoles aarch64 : RG40XX-H, etc.)

Compile `crusader-no-regret` pour Linux aarch64 (glibc ≥ 2.28) depuis Windows ou Linux, avec Zig comme compilateur croisé. Le binaire utilise le SDL2 du système de la console (muOS, Knulli, ROCKNIX, ArkOS…) ; libc++ est liée statiquement.

## Compiler

```
pip install ziglang ninja
python tools/portmaster/build.py          # -> build-portmaster/crusader-no-regret
```

`build.py` récupère les en-têtes SDL2, fabrique une `libSDL2-2.0.so.0` vide (symboles seulement) pour l'édition de liens, puis lance CMake avec `zig-aarch64.cmake`.

## Installer sur la console

Le zip de release suit la structure PortMaster (installé automatiquement par PortMaster). En manuel (USB ou lecteur de carte), le contenu du zip se répartit ainsi :

| Dans le zip | À copier vers (carte SD des ports) | Rôle |
|---|---|---|
| `Crusader No Regret.sh` | `ROMS/Ports/Crusader No Regret.sh` | lanceur, affiché dans le menu Ports |
| `crusader-no-regret/` (dossier entier) | `ports/crusader-no-regret/` | binaire, licences, artwork, données du jeu |
| `port.json`, `gameinfo.xml`, `README.md`, `cover.png`, `screenshot.png` | nulle part | métadonnées PortMaster, inutiles sur la console |

Exemple sur la RG40XX-H sous muOS (les ports sont sur la carte **SD2** ; visible en USB/MTP sous « Ce PC\RG40XX-H\SD2 ») :

```
SD2/
├── ROMS/Ports/
│   └── Crusader No Regret.sh       lanceur
└── ports/crusader-no-regret/
    ├── crusader-no-regret          binaire
    ├── licenses/                   licences
    ├── data/                       ICI : données du jeu d'origine
    ├── conf/                       créé au premier lancement
    └── log.txt                     créé au premier lancement
```

Si les ports sont sur la carte SD1, même arborescence sur SD1. Redémarrer l'interface si l'entrée Crusader No Regret n'apparaît pas dans la liste des Ports.

### Données du jeu d'origine

Elles sont à fournir soi-même (aucun fichier du jeu n'est livré) et se déposent dans **`ports/crusader-no-regret/data/`**, à côté du binaire `crusader-no-regret`, soit :

- les fichiers du jeu installé, directement dans `data/` (sans sous-dossier) :

  ```
  ports/crusader-no-regret/data/
  ├── STATIC/   USECODE/   SOUND/   FLICS/   IDATA/
  └── REGRET.EXE
  ```

- ou l'image du CD : le fichier `.iso` dans `data/` (extrait au premier lancement, puis plus nécessaire).

Versions gérées : anglaise 1.06 et allemande 1.07. Le jeu écrit ses sauvegardes (`SAVEGAME/`) et `CRUSADER.CFG` dans `data/` : lors d'une mise à jour, conserver ce dossier (ne remplacer que `crusader-no-regret/crusader-no-regret` et `ROMS/Ports/Crusader No Regret.sh`). `scripts/update-rg40xx.ps1` fait cela et copie aussi les `data/SAVEGAME/SAVEGAME.<n>` du PC sur ceux de la console (`-SkipSaves` pour garder les sauvegardes de la console). Si `data/` est vide, le lanceur affiche « Game data missing » et quitte (voir `log.txt`). Les anciennes versions s'installaient dans `ports/crusader/` avec `Crusader.sh` : renommer ce dossier en `ports/crusader-no-regret/` et supprimer l'ancien binaire `crusader` et l'ancien lanceur (le dossier PortMaster nomme l'épisode pour que le port de No Remorse puisse cohabiter).

Les sources du port sont dans `tools/portmaster/port/` (`port.json`, `README.md`, `gameinfo.xml`, lanceur, `cover.png`, `screenshot.png`).

### Artwork

- `screenshot.png` : capture 640×480 en jeu à l'échelle native (ce qu'affiche la console).
- `cover.png` : 400×300 sur fond transparent (jeu + jaquette), le format des covers PortMaster.

Les deux sont générés depuis `docs/images/` par `python tools/portmaster/make_art.py . <x> <y> tools/portmaster/port`, où `<x> <y>` est le centre du cadrage dans `docs/images/screenshot.png` (1720 700 : le Silencer).

Le front-end muOS ne les lit pas dans le port : il affiche les images de son catalogue, nommées comme le lanceur (`MUOS/info/catalogue/External - Ports/box/Crusader No Regret.png` pour la liste, `preview/` pour l'aperçu). PortMaster y copie l'`<image>` de `gameinfo.xml` (`./crusader-no-regret/cover.png`, d'où la copie dans le dossier du port) et le `screenshot.png` posé à côté. Après une installation manuelle, `scripts/update-rg40xx.ps1` les y copie lui-même.

## Manette

Le jeu lit la manette lui-même (SDL_GameController) : `gptokeyb` n'est pas utilisé (ses touches par défaut doublaient chaque entrée : RT envoyait aussi Fin, qui fait reculer). Select + Start quitte le jeu. `CRUSADER_PAD_DEBUG=1` journalise la manette dans `log.txt`. Les réglages par défaut sont ceux de l'écran « MANETTE » des options, où chaque action se réassigne (écrits dans `data/CRUSADER.CFG`, clés `padmap`, `padaim`, `paddeadzone`) :

| Entrée | Action |
|---|---|
| Stick gauche | flèches (déplacement, rotation) |
| Stick droit haut / bas | visée (souris) |
| Stick droit gauche / droite | roulade gauche / droite (Ctrl + gauche / droite) |
| RT | tir |
| A / B / X / Y | sélectionner (S) / saut / valider / arme suivante |
| LB / RB | pas à gauche / à droite (Alt + gauche / droite) |
| L3 / R3 | Maj / se baisser ou se lever (Ctrl + bas) |
| Croix | utiliser l'objet / trousse / objet précédent / objet suivant |
| Start | menu |

Dans les menus : croix = flèches, stick droit = souris, A = Entrée, B et Start = Échap.

Sauvegarde : faute de clavier pour taper un nom, le lanceur passe `--auto-save-names` : choisir un emplacement sur l'écran de sauvegarde propose la date et l'heure (`12/11/2026 14:32`), A sauvegarde sous ce nom, B annule.
