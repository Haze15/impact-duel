# IMPACT DUEL

Jeu de combat 2D en **C++20** avec **raylib** : 4 personnages, chacun avec 10 coups (normaux debout, accroupis, aériens, 3 spéciaux, 1 super), et un **serveur en ligne** qui fait autorité.

## Les combattants

| Perso | Style | Vie | Spéciaux (C / Avant+C / Bas+C) | Super (D) |
|---|---|---|---|---|
| **KAEL** | Équilibré | 1000 | Onde de choc (projectile) · Poing tornade (charge) · Uppercut du dragon (invincible, anti-air) | Rayon du dragon (rayon 5 coups) |
| **BRAKKA** | Lutteur | 1200 | Saisie de l'ours (imparable) · Charge du taureau · Séisme (coup bas autour de lui) | Suplex titanesque (saisie 380) |
| **NYX** | Ninja rapide | 850 | Shuriken (aussi en l'air) · Pas de l'ombre (traverse, invincible) · Téléportation derrière l'adversaire | Mille lames (8 coups) |
| **VOLT** | Zoneur | 900 | Orbe statique (lent, 3 coups) · Lance électrique (très longue portée) · Pilier de foudre (anti-air) | Tempête (zone énorme) |

Chaque perso a aussi : A (léger), B (lourd), Bas+A, Bas+B (balayette = fait tomber), Air+A, Air+B. La liste complète s'affiche sur l'écran de sélection.

## Système de combat

- Frame data réelle (démarrage / actif / récupération), hitbox et hurtbox, gel d'impact, tremblement d'écran.
- **Garde** : maintenir arrière. Les coups **bas** se parent accroupi, les **attaques sautées** se parent debout.
- **Saisies** imparables, mais battues si l'adversaire frappe au même moment.
- **Annulations** : un coup normal qui touche peut être annulé en spécial ou en super → combos.
- Réduction des dégâts en combo, jongles, chutes au sol avec invincibilité au relevé.
- Jauge de super (frapper et encaisser la remplit), dégâts de garde sur les spéciaux.
- Matchs en 2 manches gagnantes, chrono de 99 s, tampon d'entrées.

## Modes

- **Versus local** (2 joueurs, même clavier ou manettes)
- **Contre l'ordi** (Facile / Normal / Difficile)
- **Entraînement** (vie et jauge infinies, mannequin immobile / garde / IA, F1 = hitboxes)
- **En ligne** via le serveur

## Commandes

| | Joueur 1 | Joueur 2 |
|---|---|---|
| Déplacements | Z Q S D (AZERTY) / W A S D (QWERTY) | Flèches |
| A — léger | F | Pavé num 1 ou `;` |
| B — lourd | G | Pavé num 2 ou `:` |
| C — spécial | H | Pavé num 3 ou `!` |
| D — super | T | Pavé num 0 ou Maj droite |

Manette : A / X / B / Y (ou gâchette droite pour la super). F1 hitboxes, F11 plein écran, Échap pause.

## Compiler

Il faut CMake ≥ 3.16 et un compilateur C++20. raylib est téléchargé automatiquement.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Linux : installer d'abord `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev`.

Exécutables produits : `impact_duel` (le jeu) et `impact_server` (le serveur). Les binaires Windows / Linux / macOS sont aussi construits automatiquement par GitHub Actions à chaque push (onglet **Actions → Artifacts**) ; pousser un tag `v1.0` crée une **Release** avec les zips.

Tests : `ctest --test-dir build -C Release`.

## Jouer en ligne

GitHub héberge le code, mais ne peut pas faire tourner un serveur de jeu : le serveur doit tourner sur un PC ou un VPS.

1. Lancer le serveur : `impact_server 7777` (ou avec Docker : `docker build -t impact-server . && docker run -p 7777:7777/udp impact-server`).
2. Ouvrir le **port UDP 7777** (pare-feu, et redirection de port sur la box si c'est chez toi).
3. Les deux joueurs : menu **En ligne**, entrer `ip-du-serveur:7777`, choisir un perso.

Le serveur simule le combat à 60 Hz et envoie l'état aux deux joueurs, ce qui empêche la triche côté client. Une revanche démarre automatiquement après chaque match.

## Architecture

```
src/core/      simulation (persos, coups, physique, collisions), protocole réseau
src/net/       sockets UDP multiplateformes
src/server/    serveur de match
src/client/    rendu raylib, menus, IA, audio généré, client réseau
tests/         tests de simulation + client complet sans écran
```

Pour ajouter un perso : écrire une fonction `makeXxx()` dans `src/core/characters.cpp` et l'ajouter au `roster()`.
