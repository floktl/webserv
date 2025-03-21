<?php
/**
 * uploadFile.php
 * Nimmt eine hochgeladene Datei entgegen und speichert sie
 * in das Verzeichnis, das in $_SERVER["UPLOAD_STORE"] angegeben ist.
 *
 */

$uploadStore = isset($_SERVER["UPLOAD_STORE"]) ? $_SERVER["UPLOAD_STORE"] : "/uploads";

$targetDir = __DIR__ . $uploadStore;

if (!is_dir($targetDir)) {
	mkdir($targetDir, 0777, true);
}

if (!isset($_FILES["uploadedFile"])) {
	echo "<p>Keine Datei in 'uploadedFile' hochgeladen.</p>";
	exit;
}

if ($_FILES["uploadedFile"]["error"] === UPLOAD_ERR_OK) {
	$tmpName = $_FILES["uploadedFile"]["tmp_name"];
	$originalName = basename($_FILES["uploadedFile"]["name"]);
	$targetPath = $targetDir . "/" . $originalName;

	if (move_uploaded_file($tmpName, $targetPath)) {
	} else {
		echo "<p>Fehler beim Verschieben der Datei!</p>";
		exit;
	}
} else {
	echo "<p>Datei-Upload fehlgeschlagen. Fehlercode: {$_FILES['uploadedFile']['error']}</p>";
	exit;
}

header("Location: /");
exit;
