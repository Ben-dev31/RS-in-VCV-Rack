# Tutoriel VCV Rack

## Installation de VCV Rack

### Sur Ubuntu

1. **Installer les dépendances nécessaires :**

   ```bash
   sudo apt install unzip git gdb curl cmake libx11-dev libglu1-mesa-dev \
   libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zlib1g-dev \
   libasound2-dev libgtk2.0-dev libgtk-3-dev libjack-jackd2-dev jq zstd \
   libpulse-dev pkg-config
   ```

2. **Cloner le dépôt de VCV Rack :**

   ```bash
   git clone https://github.com/VCVRack/Rack.git
   cd Rack
   ```

3. **Initialiser les sous-modules :**

   ```bash
   git submodule update --init --recursive
   ```

4. **Compiler VCV Rack :**

   ```bash
   make dep
   make
   make run  # Lance VCV Rack
   ```

---

## Installation du SDK de VCV Rack

### Option 1 : Cloner depuis GitHub

```bash
git clone https://github.com/VCVRack/Fundamental.git
cd Fundamental
git submodule update --init --recursive
```

### Option 2 : Télécharger le SDK

- Télécharger depuis ce lien :  
  [Rack SDK - version Linux x64](https://vcvrack.com/downloads/Rack-SDK-latest-lin-x64.zip)
- Décompresser le fichier ZIP.
- Définir la variable d’environnement `RACK_DIR` :

   ```bash
   export RACK_DIR=/chemin/vers/le/dossier/dezippe
   ```

### Compilation

```bash
make dep
make
```

---

## Développement d’un plugin

### Définir le chemin du SDK dans `.bashrc`

1. Ouvrir le fichier :

   ```bash
   vim ~/.bashrc
   ```

2. Ajouter à la fin du fichier :

   ```bash
   export RACK_DIR=/chemin/vers/le/dossier/du/sdk
   ```

3. Recharger la configuration :

   ```bash
   source ~/.bashrc
   ```

---

### Création d’un plugin à l’aide du template

Utiliser l’outil `helper.py` :

```bash
$RACK_DIR/helper.py createplugin MyPlugin
```

- Remplacer `MyPlugin` par le nom de votre plugin.
- Répondre aux questions posées.

Cette commande crée un dossier `MyPlugin` avec la structure suivante :

```
MyPlugin/
├── res/          # Contiendra le fichier SVG (interface graphique)
├── src/
│   ├── plugin.cpp
│   └── plugin.hpp
├── Makefile
└── plugin.json
```

### Modifications nécessaires après génération :

- **Décommenter** la ligne suivante dans `plugin.cpp` :
  
  ```cpp
  p->addModel(modelMyModule);
  ```

- **Décommenter** aussi cette ligne dans `plugin.hpp` :

  ```cpp
  extern Model* modelMyModule;
  ```

---

### Ajouter un fichier SVG

- Placer le fichier SVG dans le dossier `res/`.
- Vérifier dans Inkscape (ou un autre éditeur) que les éléments graphiques sont bien nommés avec les bonnes couleurs.
  - Exemple : `Knob`, `Input`, `Output`, etc.

- **Param**: red #ff0000
- **Input**: green #00ff00
- **Output**: blue #0000ff
- **Light**: magenta #ff00ff
- **Custom widgets**: yellow #ffff00

---

## Création de module

```bash
cd MyPlugin
```
Puis
```bash
$RACK_DIR/helper.py createmodule MyModule res/MyModule.svg src/MyModule.cpp
 ```
Cette commande vas créer les fichiers cpp automatiquement en fonction des composants trouvés dans le svg.


## Compilation du plugin

Dans le dossier du plugin :

```bash
make install
```
Puis lancer VCV Rack (le plugin sera disponible s’il est dans le bon dossier `plugins` ou `plugins-v2`).


# Les composants graphique

### Knobs (Potentiomètres)
| Nom du composant      | Taille (px) | Description                                                |
| --------------------- | ----------- | ---------------------------------------------------------- |
| `RoundHugeBlackKnob`  | 38x38       | Grosse molette noire, utile pour les paramètres principaux |
| `RoundLargeBlackKnob` | 32x32       | Molette de taille moyenne                                  |
| `RoundBlackKnob`      | 25x25       | Molette standard                                           |
| `RoundSmallBlackKnob` | 17x17       | Petite molette, pour paramètres secondaires                |
| `Trimpot`             | 9x9         | Molette minuscule pour ajustements fins                    |

- Base class : `SvgKnob`\
- Méthode de création : `createParamCentered<...>()`

### ⬛ Boutons (Buttons)

| Nom du composant  | Taille (px) | Description                                              |
| ----------------- | ----------- | -------------------------------------------------------- |
| `LEDButton`       | 15x15 env.  | Bouton cliquable avec LED intégrée (toggle ou momentary) |
| `CKD6`            | 9x9         | Petit switch carré (toggle)                              |
| `CKSS`            | 9x12        | Interrupteur à deux positions                            |
| `CKSSThree`       | 9x12        | Interrupteur à trois positions                           |
| `MomentarySwitch` | 10x10 env.  | Bouton poussoir temporaire                               |

- Base class : `SvgSwitch`\
- Méthode de création : `createParamCentered<...>()`

### 🔌 Ports

| Nom du composant | Taille (px) | Description                                                 |
| ---------------- | ----------- | ----------------------------------------------------------- |
| `PJ301MPort`     | 15x15       | Port jack classique pour entrée ou sortie (CV, audio, gate) |

- Base class : `SvgPort`
- Méthode de création : `createInputCentered<...>()` ou `createOutputCentered<...>()`

### 💡 LEDs (Lumières)

| Nom              | Taille approx. | Description            |
| ---------------- | -------------- | ---------------------- |
| `TinyLight`      | 2x2            | LED minuscule          |
| `SmallLight`     | 5x5            | LED standard           |
| `MediumLight`    | 7x7            | LED un peu plus grande |
| `LargeLight`     | 9x9            | LED visible à distance |
| `VeryLargeLight` | 11x11          | LED très grande        |

#### Types de couleurs (template) :

   - `RedLight`

   - `GreenLight`

   - `BlueLight`

   - `YellowLight`

   - `WhiteLight`

   - `OrangeLight`

   - `PurpleLight`

   - `CyanLight`

- Base class : `ModuleLightWidget`
- Méthode de création : `createLightCentered<...>()`

### 🧩 Vis / Visuels divers
| Nom du composant | Taille (px) | Description              |
| ---------------- | ----------- | ------------------------ |
| `ScrewSilver`    | 7x7         | Vis d'apparence argentée |
| `ScrewBlack`     | 7x7         | Vis noire                |

- Base class : `Widget`
- Méthode de création : `createWidget<...>()`


## 📦 Classes de base (héritage)
| Classe de base      | Description                               |
| ------------------- | ----------------------------------------- |
| `SvgKnob`           | Base pour les knobs                       |
| `SvgSwitch`         | Base pour les boutons/interrupteurs       |
| `SvgPort`           | Base pour les ports d'entrée/sortie       |
| `ModuleLightWidget` | Base pour les LEDs                        |
| `Widget`            | Base générique pour composants graphiques |

## 🛠️ Exemple d'utilisation
``` cpp
addParam(createParamCentered<RoundBlackKnob>(
    mm2px(Vec(15.0, 40.0)), module, MyModule::PARAM_ID));

addInput(createInputCentered<PJ301MPort>(
    mm2px(Vec(5.0, 100.0)), module, MyModule::INPUT_ID));

addOutput(createOutputCentered<PJ301MPort>(
    mm2px(Vec(25.0, 100.0)), module, MyModule::OUTPUT_ID));

addChild(createLightCentered<SmallLight<RedLight>>(
    mm2px(Vec(15.0, 70.0)), module, MyModule::LIGHT_ID));

```

# Ressources utiles

- [Documentation officielle de VCV Rack](https://vcvrack.com/manual/)
- [Exemples de plugins](https://github.com/VCVRack/VCV-Prototype)


