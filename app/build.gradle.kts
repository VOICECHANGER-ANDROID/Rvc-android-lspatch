// Ce fichier doit être dans app/build.gradle.kts
plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "com.rvc.module" // Le nom du package
    compileSdk = 34

    defaultConfig {
        applicationId = "com.rvc.module"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "1.0-ULTIME"

        // Configuration pour le NDK (C++)
        externalNativeBuild {
            cmake {
                cppFlags += "" // Options de compilation globales
            }
        }
    }
    
    // Configuration pour le NDK : Lie le projet à CMakeLists.txt
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
    
    // Assure que l'APK inclut les fichiers de métadonnées pour LSPatch
    packaging {
        jniLibs {
            // S'assurer que les bibliothèques NDK (librvc_main_engine.so) sont dans l'APK
            useLegacyPackaging = true
        }
        resources {
            // Inclure xposed_init dans le dossier assets
            excludes.add("/META-INF/{AL2.0,LGPL2.1}")
        }
    }
}

dependencies {
    // Dépendance pour le framework de hook LSPosed (crucial)
    compileOnly("de.robv.android.xposed:api:82") // Version courante Xposed

    // Dépendances Kotlin/Android
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("com.google.code.gson:gson:2.10.1") // Pour ProfileManager/SharedPreferences
    
    // Vous devez ajouter ici les dépendances pour Oboe, TFLite et ONNX si vous les utilisez via Gradle
    // Par exemple:
    // implementation("com.google.tflite:tensorflow-lite-select-tf-ops:2.15.0")
}
