<?php
/**
 * uploadFile.php
 * Nimmt eine hochgeladene Datei entgegen und speichert sie
 * in das Verzeichnis, das in $_SERVER["UPLOAD_STORE"] angegeben ist.
 *
 * Danach leiten wir zurück zur Hauptseite.
 */

// Pfad aus SERVER-Variable holen (z.B. "/upload" oder "/myfiles")
$uploadStore = isset($_SERVER["UPLOAD_STORE"]) ? $_SERVER["UPLOAD_STORE"] : "/upload";

// Volles Verzeichnis relativ zu diesem PHP-Script
$targetDir = __DIR__ . $uploadStore;

// Prüfen, ob Verzeichnis existiert, ansonsten anlegen
if (!is_dir($targetDir)) {
	mkdir($targetDir, 0777, true);
}

// Checken, ob überhaupt eine Datei im Formularfeld "uploadedFile" kam
if (!isset($_FILES["uploadedFile"])) {
	echo "<p>Keine Datei in 'uploadedFile' hochgeladen.</p>";
	exit;
}

// Fehlerstatus prüfen
if ($_FILES["uploadedFile"]["error"] === UPLOAD_ERR_OK) {
	// Temporäre Datei und Zielname
	$tmpName = $_FILES["uploadedFile"]["tmp_name"];
	$originalName = basename($_FILES["uploadedFile"]["name"]);
	$targetPath = $targetDir . "/" . $originalName;

	// Datei verschieben
	if (move_uploaded_file($tmpName, $targetPath)) {
		// Erfolg
		// Hier könnte man auch eine Ausgabe machen
		//echo "Datei erfolgreich hochgeladen nach $targetPath";
	} else {
		// Fehler beim Verschieben
		echo "<p>Fehler beim Verschieben der Datei!</p>";
		exit;
	}
} else {
	// Wenn z.B. UPLOAD_ERR_INI_SIZE, UPLOAD_ERR_FORM_SIZE, etc. auftritt
	echo "<p>Datei-Upload fehlgeschlagen. Fehlercode: {$_FILES['uploadedFile']['error']}</p>";
	exit;
}

// Am Ende zurück zur Startseite leiten (kannst du anpassen)
header("Location: /");
exit;
