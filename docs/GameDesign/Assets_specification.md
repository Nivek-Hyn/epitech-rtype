> Historical document from the project. After the project, the sprite sheets below were replaced by graphics drawn in code (see [client.md](../client.md)).

1. Introduction

Ce document répertorie le mapping officiel entre les entités logiques du jeu (définies dans protocol.hpp et Components.hpp) et les assets graphiques du dossier assets/ pour le projet R-Type.

2. Entités Joueurs (EntityType::Player)

Le serveur gère jusqu'à 4 joueurs simultanément (MaxPlayers = 4).
Pour respecter la règle du sujet (les 4 joueurs doivent être distincts), nous utilisons la feuille de sprites assets/r-typesheet42.gif.


le fichier utilisé pour les jouerus est : 

r-typesheet42;gif

Dessus il y a 5 sprites, le sujet en demande 4 masi ca peut faire objet d'un bonus 





3. Ennemi de Base / Bydos (EntityType::Enemy)

Pour le prototype (Partie 1), le premier ennemi standard est le vaisseau ennemi classique de R-Type.


Pour le prmeier vaisseau ennemy, on va prendre le:

r-typesheet5.gif

Apparition sur le bord droit, déplacement horizontal vers la gauche (avec pattern de vol en boucle).


