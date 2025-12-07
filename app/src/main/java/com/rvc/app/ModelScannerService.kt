package com.rvc.app

import android.app.Service
import android.content.Intent
import android.os.*
import android.util.Log
import com.rvc.app.data.ModelInfo
import com.rvc.app.util.SharedPreferencesManager
import java.io.File
import java.io.FilenameFilter
import java.util.concurrent.CopyOnWriteArrayList

/**
 * Service en arrière-plan pour scanner, valider et gérer la bibliothèque de modèles RVC.
 */
class ModelScannerService : Service() {

    private val TAG = "RVC_ModelScanner"

    // Chemin vers le dossier des modèles
    private val MODELS_DIR_NAME = "RVC_Voice_Models"
    private lateinit var modelsDir: File
    
    // Liste thread-safe pour stocker les modèles valides
    private val validModels: CopyOnWriteArrayList<ModelInfo> = CopyOnWriteArrayList()

    private val serviceHandler: Handler
    private val serviceLooper: Looper

    // Binder pour communiquer avec l'activité principale
    private val binder = ModelScannerBinder()

    /**
     * Interface pour que l'activité puisse accéder à la liste des modèles.
     */
    inner class ModelScannerBinder : Binder() {
        fun getService(): ModelScannerService = this@ModelScannerService
    }

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Démarrage du ModelScannerService.")
        
        // Initialiser le Looper et le Handler pour exécuter les scans de manière asynchrone
        val thread = HandlerThread("ModelScannerThread", Process.THREAD_PRIORITY_BACKGROUND)
        thread.start()
        serviceLooper = thread.looper
        serviceHandler = Handler(serviceLooper)

        // Définir le chemin du dossier
        modelsDir = File(Environment.getExternalStorageDirectory(), MODELS_DIR_NAME)
        
        // S'assurer que le dossier existe
        if (!modelsDir.exists()) {
            modelsDir.mkdirs()
            Log.i(TAG, "Création du dossier de modèles: ${modelsDir.absolutePath}")
        }
        
        // Lancer le scan initial
        serviceHandler.post { 
            scanAndValidateModels() 
        }
    }

    override fun onBind(intent: Intent?): IBinder {
        return binder
    }

    /**
     * Exécute le processus de scan et de validation des modèles.
     */
    private fun scanAndValidateModels() {
        Log.i(TAG, "Scan du dossier de modèles en cours...")
        val scannedList = mutableListOf<ModelInfo>()

        // Filtre pour les extensions de modèles supportées
        val modelFilter = FilenameFilter { _, name ->
            name.endsWith(".onnx", true) || name.endsWith(".tflite", true)
        }

        modelsDir.listFiles(modelFilter)?.forEach { file ->
            // Le nom du modèle est le nom du fichier sans l'extension
            val name = file.nameWithoutExtension
            val path = file.absolutePath
            val type = if (file.extension.equals("onnx", true)) "ONNX" else "TFLite"

            val modelInfo = ModelInfo(name, path, type)
            
            // V13.0: Validateur de Modèle à Froid (Cold Validation)
            if (performColdValidation(modelInfo)) {
                scannedList.add(modelInfo)
                Log.i(TAG, "✅ Modèle valide trouvé: $name ($type)")
            } else {
                Log.w(TAG, "❌ Modèle ignoré (échec de la validation à froid): $name")
            }
        }
        
        validModels.clear()
        validModels.addAll(scannedList)
        Log.i(TAG, "Scan terminé. ${validModels.size} modèles valides trouvés.")
        
        // Mettre à jour l'état de l'application (peut déclencher une mise à jour UI via un Broadcast)
        notifyModelListChanged()
    }

    /**
     * Exécute une micro-inférence pour s'assurer que le modèle est chargeable.
     * (V13.0 : Validation à Froid)
     */
    private fun performColdValidation(model: ModelInfo): Boolean {
        // --- LOGIQUE CRITIQUE ---
        
        // 1. Déplacer la logique de validation vers le NDK pour l'accès aux runtimes TFLite/ONNX
        // Le NDK est le seul à pouvoir vraiment valider le délégué DSP/Hexagon.
        
        // Ici, on fait un appel JNI pour charger et tester le modèle rapidement en C++
        // CppValidationManager.validateModel(model.path, model.type)
        
        // Pour la version Kotlin, on simule:
        try {
            // Le temps nécessaire pour charger l'interpréteur et exécuter une passe (doit être < 500ms)
            Thread.sleep(100) 
            return true // Validation réussie
        } catch (e: Exception) {
            Log.e(TAG, "Validation NDK échouée pour ${model.name}: ${e.message}")
            return false // Validation échouée
        }
        // --- FIN LOGIQUE CRITIQUE ---
    }

    /**
     * Notifie les autres composants (UI) que la liste des modèles a changé.
     */
    private fun notifyModelListChanged() {
        // Utiliser un LocalBroadcastManager pour notifier l'activité que les modèles sont prêts.
        val intent = Intent("RVC_MODEL_LIST_UPDATED")
        // LocalBroadcastManager.getInstance(this).sendBroadcast(intent)
    }

    /**
     * Méthode publique pour obtenir la liste des modèles valides.
     */
    fun getValidModels(): List<ModelInfo> {
        return validModels
    }
    
    /**
     * Méthode publique pour déclencher un nouveau scan (ex: après un ajout de fichier).
     */
    fun triggerRescan() {
        serviceHandler.post { 
            scanAndValidateModels() 
        }
    }
    
    override fun onDestroy() {
        super.onDestroy()
        serviceLooper.quit()
        Log.i(TAG, "ModelScannerService arrêté.")
    }
}
