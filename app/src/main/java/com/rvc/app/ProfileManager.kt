package com.rvc.app.util

import android.content.Context
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import com.rvc.app.data.Profile

/**
 * Gère la sauvegarde et le chargement des profils RVC pour des applications spécifiques.
 * Utilise SharedPreferences pour la persistance locale des données.
 */
object ProfileManager {

    private const val PREFS_NAME = "RVC_Profiles"
    private const val KEY_PROFILES_MAP = "profiles_map"

    private val gson = Gson()

    /**
     * Charge tous les profils existants à partir des préférences.
     */
    private fun loadAllProfiles(context: Context): MutableMap<String, Profile> {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val json = prefs.getString(KEY_PROFILES_MAP, null)

        return if (json != null) {
            // Utilisation de TypeToken pour désérialiser correctement la Map générique
            val type = object : TypeToken<MutableMap<String, Profile>>() {}.type
            gson.fromJson(json, type)
        } else {
            mutableMapOf()
        }
    }

    /**
     * Sauvegarde la Map complète des profils dans les préférences.
     */
    private fun saveAllProfiles(context: Context, profilesMap: Map<String, Profile>) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val json = gson.toJson(profilesMap)
        prefs.edit().putString(KEY_PROFILES_MAP, json).apply()
    }

    /**
     * Crée ou met à jour un profil pour un package d'application donné.
     * * @param packageName Le nom du package de l'application (ex: com.discord).
     * @param profile Le profil RVC à sauvegarder.
     */
    fun saveProfile(context: Context, packageName: String, profile: Profile) {
        val profilesMap = loadAllProfiles(context)
        profilesMap[packageName] = profile
        saveAllProfiles(context, profilesMap)
        // Note: Un mécanisme de notification (Broadcast) serait idéal ici pour informer le Hook
        // de la mise à jour des règles d'exclusion/injection.
    }

    /**
     * Charge le profil associé à un package d'application.
     * * @param packageName Le nom du package de l'application.
     * @return Le profil RVC, ou null si aucun profil n'existe pour ce package.
     */
    fun loadProfile(context: Context, packageName: String): Profile? {
        val profilesMap = loadAllProfiles(context)
        return profilesMap[packageName]
    }

    /**
     * Supprime un profil existant.
     */
    fun deleteProfile(context: Context, packageName: String) {
        val profilesMap = loadAllProfiles(context)
        if (profilesMap.remove(packageName) != null) {
            saveAllProfiles(context, profilesMap)
        }
    }

    /**
     * Obtient une liste de tous les packages d'applications ayant un profil sauvegardé.
     */
    fun getAllProfilePackageNames(context: Context): Set<String> {
        return loadAllProfiles(context).keys
    }
    
    // --- Logique pour la Liste d'Exclusion (V10.0) ---

    /**
     * Vérifie si le RVC est désactivé pour un package donné (Liste Noire).
     * * Cette fonction serait principalement appelée par la classe HookEntry.kt (via le JNI/IPC).
     */
    fun isExcluded(context: Context, packageName: String): Boolean {
        // Logique simplifiée: si un profil existe et est marqué comme désactivé par défaut, ou 
        // si le package est dans la liste d'exclusion globale.
        val profile = loadProfile(context, packageName)
        return profile?.isExcluded ?: false
    }
}
