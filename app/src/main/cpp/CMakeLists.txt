# Spécifie la version minimale de CMake requise pour construire le projet.
cmake_minimum_required(VERSION 3.22.1)

# Nomme le projet NDK.
project("rvc_module_engine")

# ----------------------------------------------------------------------------------
# I. Déclaration des Variables et des Chemins
# ----------------------------------------------------------------------------------

# Chemins vers les sources C++
set(RVC_SOURCES
    rvc_engine.cpp
    dsp/fx_graph.cpp
    inference/ie_manager.cpp
    security/lock_manager.cpp
    audio/oboe_duplex.cpp
)

# Définit le chemin pour les dépendances externes (TFLite, ONNX RT, Oboe)
# Ces dépendances sont souvent placées dans un répertoire 'prebuilt' ou 'third_party'.
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# ----------------------------------------------------------------------------------
# II. Intégration des Dépendances Critiques
# ----------------------------------------------------------------------------------

# 1. Bibliothèque Oboe (pour l'audio temps réel et le Sidetone)
# Oboe est généralement fourni par Android Studio (via l'AAR ou un CMake FindPackage)
# Ici, nous supposons qu'Oboe est déjà disponible ou dans le répertoire third_party.
# Si vous utilisez la méthode du paquet fourni par Android Studio :
find_package(oboe REQUIRED)

# 2. TensorFlow Lite (Pour les modèles .tflite et le Délégué Hexagon/DSP)
# Nous supposons que les fichiers TFLite sont pré-construits pour ARM64.
add_library(
    tflite_runtime
    STATIC
    IMPORTED
)
set_target_properties(tflite_runtime
    PROPERTIES
    IMPORTED_LOCATION "${THIRD_PARTY_DIR}/tflite/lib/libtensorflowlite.a"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/tflite/include"
)

# 3. ONNX Runtime (Pour les modèles .onnx et l'accélération GPU/DSP)
# De même, nous supposons que les fichiers ORT sont pré-construits.
add_library(
    onnxruntime_mobile
    STATIC
    IMPORTED
)
set_target_properties(onnxruntime_mobile
    PROPERTIES
    IMPORTED_LOCATION "${THIRD_PARTY_DIR}/onnxruntime/lib/libonnxruntime.a"
    INTERFACE_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/onnxruntime/include"
)

# ----------------------------------------------------------------------------------
# III. Définition et Compilation de la Bibliothèque Principale (rvc_main_engine)
# ----------------------------------------------------------------------------------

# Crée la bibliothèque partagée (lib[rvc_main_engine].so) qui sera chargée par System.loadLibrary("rvc_main_engine") en Kotlin.
add_library(
    rvc_main_engine
    SHARED
    ${RVC_SOURCES}
)

# ----------------------------------------------------------------------------------
# IV. Configuration des Options de Compilation et de Liaison
# ----------------------------------------------------------------------------------

# Indique au compilateur C++ d'utiliser la norme la plus récente (C++20 pour les fonctionnalités modernes)
target_compile_features(rvc_main_engine PRIVATE cxx_std_20)

# Options de compilation pour la performance et la stabilité.
target_compile_options(rvc_main_engine PRIVATE
    -Wall
    -Werror  # Traite les warnings comme des erreurs pour une stabilité maximale
    -fPIC
    -O3      # Optimisation maximale pour la vitesse
    # Options pour le temps réel et l'optimisation ARM
    -D_FORTIFY_SOURCE=2
    -ffast-math 
)

# Inclut les chemins nécessaires pour les dépendances.
target_include_directories(rvc_main_engine PRIVATE
    ${THIRD_PARTY_DIR}/rvc_core/include # Pour vos headers RVC internes
    ${THIRD_PARTY_DIR}/oboe/include
    ${THIRD_PARTY_DIR}/tflite/include
    ${THIRD_PARTY_DIR}/onnxruntime/include
)

# Lie les dépendances à la bibliothèque principale.
target_link_libraries(rvc_main_engine
    # Dépendances système
    android
    log             # Pour les logs NDK (Logcat)
    z               # ZLib (souvent requis par TFLite/ONNX RT)
    m               # Fonctions mathématiques
    
    # Dépendances critiques RVC/Audio
    oboe
    tflite_runtime
    onnxruntime_mobile
)
