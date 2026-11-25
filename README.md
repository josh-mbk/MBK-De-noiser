# MBK De-noiser

Réducteur de bruit spectral **VST3** (Windows & macOS), construit avec [JUCE](https://juce.com).

- **Reduction** : atténuation maximale appliquée au bruit (0–40 dB)
- **Threshold** : agressivité de la détection du bruit (0–20 dB au-dessus du bruit estimé)
- **Smoothing** : lissage temporel/fréquentiel (réduit le « bruit musical »)
- **Mix** / **Output** : dry/wet et gain de sortie
- **Mode Adaptive** : estimation automatique du bruit de fond
- **Mode Learned** + bouton **LEARN NOISE** : apprend le bruit sur 2 s (jouez uniquement le bruit)

Latence : 2048 échantillons (rapportée à l'hôte, compensation automatique).
Le profil appris n'est pas sauvegardé avec le projet : refaites un *Learn* à la réouverture.

## Compiler en local

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MBKDenoiser_VST3
```
Le plugin est généré dans `build/MBKDenoiser_artefacts/Release/VST3/`.

## Publier une version (installeurs automatiques)

```bash
git tag v1.0.0
git push origin v1.0.0
```
GitHub Actions compile le plugin et crée une *Release* contenant :

| Fichier | Contenu |
|---|---|
| `MBK-De-noiser-1.0.0-Windows-Setup.exe` | Installeur Inno Setup → `C:\Program Files\Common Files\VST3\` |
| `MBK-De-noiser-1.0.0-macOS.pkg` | Installeur → `/Library/Audio/Plug-Ins/VST3/` |

> Les installeurs ne sont pas signés (Windows SmartScreen / macOS Gatekeeper afficheront un avertissement).
> Pour une distribution grand public, ajoutez un certificat de signature de code (Windows) et un Apple Developer ID + notarisation (macOS).
