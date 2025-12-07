// --- app/src/main/java/com/rvc/app/data/ModelInfo.kt ---
data class ModelInfo(
    val name: String,
    val path: String,
    val type: String // "ONNX" ou "TFLITE"
)

// --- app/src/main/java/com/rvc/app/data/Profile.kt ---
data class Profile(
    val packageName: String,
    val modelInfo: ModelInfo,
    val pitchValue: Int,        // -12 à 12
    val naturalityValue: Int,   // 0 à 100
    val isExcluded: Boolean = false // Si l'app est sur la liste noire (pas de RVC)
)
