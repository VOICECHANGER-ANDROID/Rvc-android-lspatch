package com.rvc.patch

import android.media.AudioRecord
import android.util.Log
import com.rvc.patch.ipc.IPCManager
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.callbacks.XC_LoadPackage
import java.nio.ByteBuffer

/**
 * Classe d'entrée principale pour le module LSPatch (Trough Mic).
 * Implémente IXposedHookLoadPackage pour intercepter les processus système.
 */
class HookEntry : IXposedHookLoadPackage {

    companion object {
        private const val TAG = "RVCHook"
        private const val TARGET_PACKAGE = "android" // Cible le processus système pour le Trough Mic
        private val ipcManager = IPCManager() // Instance pour gérer la communication NDK (Ashmem)
    }

    /**
     * Cette méthode est appelée par LSPatch pour chaque package chargé.
     * Nous ciblons uniquement le processus "android" pour le hook global.
     */
    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam) {
        if (lpparam.packageName != TARGET_PACKAGE) {
            return // N'injecter la logique que dans le processus système Android
        }

        Log.i(TAG, "🟢 RVC Module injecté dans le processus système Android.")
        
        // 1. Démarrer le Moteur NDK C++ et la communication Ashmem (Zero-Copy)
        // La gestion réelle du démarrage du service se fera ici.
        ipcManager.init(lpparam.classLoader) 

        // 2. Tenter d'intercepter la méthode de lecture (read) d'AudioRecord.
        // C'est le point où les données du microphone sont capturées avant d'atteindre l'application.
        hookAudioRecordRead(lpparam.classLoader)
    }

    /**
     * Intercepte la méthode AudioRecord.read(ByteBuffer dest, int size).
     * C'est la méthode de haute performance utilisée par Oboe/AAudio.
     */
    private fun hookAudioRecordRead(classLoader: ClassLoader) {
        try {
            val audioRecordClass = AudioRecord::class.java
            
            // On cible la méthode de lecture la plus courante (ByteBuffer)
            XposedBridge.hookMethod(
                audioRecordClass.getMethod("read", ByteBuffer::class.java, Int::class.java),
                object : XC_MethodHook() {
                    
                    // Après que le microphone ait écrit les données dans le buffer
                    override fun afterHookedMethod(param: MethodHookParam) {
                        
                        // Assurez-vous que l'appel original a réussi
                        val bytesRead = param.result as Int
                        if (bytesRead <= 0) return

                        // Le buffer original capturé du microphone
                        val audioBuffer = param.args[0] as ByteBuffer
                        
                        // Déplacer la position du buffer au début du paquet audio
                        audioBuffer.position(0)
                        
                        // *****************************************************************
                        // ** POINT CRITIQUE : DÉBUT DU TRAITEMENT TEMPS RÉEL (TROUGH MIC) **
                        // *****************************************************************

                        // 3. Envoyer les données brutes au Moteur NDK (via Ashmem)
                        // Le code NDK va lire le buffer, le traiter (RVC, Pitch, EQ, etc.) et écrire
                        // le résultat modifié directement dans la même zone Ashmem.
                        val processed = ipcManager.processAudioBuffer(audioBuffer, bytesRead)
                        
                        // Si le processus RVC est actif et a retourné un succès (true)
                        if (processed) {
                            // Le buffer contient maintenant la voix modifiée.
                            // Pas besoin de copier, car le NDK a travaillé directement dans la mémoire partagée.
                        } else {
                            // Si le RVC est désactivé ou en mode erreur, le buffer original est renvoyé (pass-through).
                        }

                        // Déplacer la position du buffer à la fin pour qu'Android puisse lire le paquet complet
                        audioBuffer.position(bytesRead)
                        
                        // *****************************************************************
                        // ** FIN DU TRAITEMENT (Latence critique de 5-20ms ici) **
                        // *****************************************************************
                    }
                })
            Log.i(TAG, "✅ Hook AudioRecord.read(ByteBuffer) réussi.")
            
        } catch (e: Exception) {
            Log.e(TAG, "❌ Échec du Hook AudioRecord: " + e.message)
            // Tentative de Hook d'autres méthodes de lecture ici si la première échoue (Stabilité V11.0)
        }
    }
}
