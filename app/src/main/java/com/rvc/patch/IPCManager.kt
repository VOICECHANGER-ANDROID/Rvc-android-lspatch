package com.rvc.patch.ipc

import android.os.IBinder
import android.os.MemoryFile
import android.os.ParcelFileDescriptor
import android.util.Log
import dalvik.system.PathClassLoader
import java.io.FileDescriptor
import java.lang.reflect.Method
import java.nio.ByteBuffer

/**
 * Gère la communication Inter-Processus (IPC) entre le Hook (processus système)
 * et le Moteur RVC NDK (via JNI et Ashmem pour le Zero-Copy).
 *
 * Cette classe utilise la réflexion pour interagir avec le Moteur RVC C++ chargé dynamiquement.
 */
class IPCManager {

    private val TAG = "RVCIpcManager"

    // La taille maximale du buffer Ashmem (ajustée aux besoins max d'AudioRecord)
    private val BUFFER_SIZE = 65536 // Exemple: 64KB, adapté à la plupart des buffers AudioRecord

    // Le FileDescriptor de la mémoire partagée (Ashmem)
    private var sharedMemoryFd: FileDescriptor? = null

    // Référence au ByteBuffer mappé pour l'accès direct en Java/Kotlin
    private var sharedByteBuffer: ByteBuffer? = null

    // Méthode JNI native pour initialiser et lier le moteur C++
    private external fun initializeNativeEngine(fd: FileDescriptor, bufferSize: Int): Boolean

    // Méthode JNI native pour traiter les données audio
    private external fun processAudioNative(bytesRead: Int): Boolean

    // Déclaration du bloc natif pour charger les bibliothèques NDK (libmain.so)
    init {
        try {
            // Assurez-vous que la bibliothèque JNI contenant les fonctions natives est chargée
            System.loadLibrary("rvc_main_engine") 
            Log.i(TAG, "Bibliothèque NDK chargée avec succès.")
        } catch (e: Exception) {
            Log.e(TAG, "Erreur de chargement de la bibliothèque NDK: ${e.message}")
        }
    }
    
    /**
     * Initialise la mémoire partagée et démarre le moteur RVC NDK.
     */
    fun init(classLoader: ClassLoader) {
        try {
            // 1. Créer la zone de mémoire partagée (Ashmem)
            val memoryFile = MemoryFile("RVC_Ashmem_Buffer", BUFFER_SIZE)
            
            // 2. Utiliser la réflexion pour obtenir le FileDescriptor (Mécanisme sécurisé)
            val getFDMethod: Method = MemoryFile::class.java.getDeclaredMethod("getFileDescriptor")
            sharedMemoryFd = getFDMethod.invoke(memoryFile) as FileDescriptor

            // 3. Mapper le MemoryFile pour l'accès direct en Java (nécessaire pour le transfert initial)
            val mapMethod: Method = MemoryFile::class.java.getDeclaredMethod("map", Int::class.java, Int::class.java, Int::class.java)
            val MAP_READ_WRITE = 2 // Constante pour l'accès en lecture/écriture
            sharedByteBuffer = mapMethod.invoke(memoryFile, MAP_READ_WRITE, 0, BUFFER_SIZE) as ByteBuffer
            
            // 4. Initialiser le Moteur C++ en lui passant le FileDescriptor (Ashmem)
            if (sharedMemoryFd != null && initializeNativeEngine(sharedMemoryFd!!, BUFFER_SIZE)) {
                Log.i(TAG, "Moteur RVC C++ initialisé et lié à Ashmem.")
            } else {
                Log.e(TAG, "Échec de l'initialisation du moteur C++.")
            }

        } catch (e: Exception) {
            Log.e(TAG, "Erreur d'initialisation IPC et Ashmem: ${e.message}")
        }
    }

    /**
     * Traite le buffer audio en utilisant la mémoire partagée.
     * C'est la fonction appelée par le HookEntry dans le processus système.
     *
     * @param sourceBuffer Le ByteBuffer provenant d'AudioRecord (micro).
     * @param bytesRead Le nombre d'octets lus.
     * @return true si le traitement RVC a eu lieu, false sinon (pass-through).
     */
    fun processAudioBuffer(sourceBuffer: ByteBuffer, bytesRead: Int): Boolean {
        if (sharedByteBuffer == null) return false

        try {
            // 1. Copier le buffer AudioRecord dans le buffer Ashmem
            // Note: C'est la seule copie de données, mais elle est minimisée au max.
            sourceBuffer.rewind()
            sharedByteBuffer!!.rewind()
            
            // Utiliser le transfert direct entre buffers si possible, ou une copie optimisée
            sourceBuffer.limit(bytesRead)
            sharedByteBuffer!!.put(sourceBuffer)
            
            // 2. Déclencher le traitement Temps Réel NDK
            // Le C++ lit le Ashmem, le traite, et réécrit dans Ashmem.
            val success = processAudioNative(bytesRead)
            
            // 3. Copier les données traitées de Ashmem vers le buffer original d'AudioRecord
            sharedByteBuffer!!.rewind()
            sourceBuffer.rewind()
            sharedByteBuffer!!.limit(bytesRead)
            sourceBuffer.put(sharedByteBuffer!!)
            
            return success
        } catch (e: Exception) {
            Log.e(TAG, "Erreur critique dans processAudioBuffer: ${e.message}")
            return false // Force le pass-through en cas d'erreur
        }
    }
}
