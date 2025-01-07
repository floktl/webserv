<?php
	error_log("=== Upload Debug Start ===");
	error_log("REQUEST_METHOD: " . $_SERVER['REQUEST_METHOD']);
	error_log("CONTENT_TYPE: " . $_SERVER['CONTENT_TYPE']);
	error_log("CONTENT_LENGTH: " . $_SERVER['CONTENT_LENGTH']);
	error_log("BOUNDARY: " . $_SERVER['BOUNDARY']);

	$raw_input = file_get_contents('php://input');
	error_log("Raw input length: " . strlen($raw_input));
	error_log("First 200 bytes: " . substr($raw_input, 0, 200));

	error_log("FILES array: " . print_r($_FILES, true));
	error_log("POST array: " . print_r($_POST, true));

	$filesDir = __DIR__ . '/files';
	error_log("Upload directory: " . $filesDir);

	if (!is_dir($filesDir)) {
		$mkdir_result = mkdir($filesDir, 0777, true);
		error_log("Created directory result: " . ($mkdir_result ? "success" : "failed"));
	}

	if (isset($_FILES['uploadedFile'])) {
		error_log("Upload details: " . print_r($_FILES['uploadedFile'], true));

		if ($_FILES['uploadedFile']['error'] === UPLOAD_ERR_OK) {
			$tmpName = $_FILES['uploadedFile']['tmp_name'];
			$originalName = basename($_FILES['uploadedFile']['name']);
			$targetPath = $filesDir . '/' . $originalName;

			error_log("Temp file: " . $tmpName);
			error_log("Target path: " . $targetPath);

			if (file_exists($tmpName)) {
				error_log("Temp file exists");
				$move_result = move_uploaded_file($tmpName, $targetPath);
				error_log("Move result: " . ($move_result ? "success" : "failed"));
				if (!$move_result) {
					error_log("Move error: " . error_get_last()['message']);
				}
			} else {
				error_log("Temp file does not exist!");
			}
		} else {
			error_log("Upload error code: " . $_FILES['uploadedFile']['error']);
		}
	} else {
		error_log("No uploadedFile in FILES array");
	}

	error_log("=== Upload Debug End ===");

	header('Location: /');
	exit();
?>