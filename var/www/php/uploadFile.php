<?php
	// uploadFile.php
	error_log(print_r($_FILES, true));

	$filesDir = __DIR__ . '/files';
	if (!is_dir($filesDir)) {
		mkdir($filesDir, 0777, true);
	}

	if (isset($_FILES['uploadedFile']) && $_FILES['uploadedFile']['error'] === UPLOAD_ERR_OK) {
		$tmpName = $_FILES['uploadedFile']['tmp_name'];
		$originalName = basename($_FILES['uploadedFile']['name']);
		$targetPath = $filesDir . '/' . $originalName;

		move_uploaded_file($tmpName, $targetPath);
	}

	header('Location: /');
	exit();
?>
