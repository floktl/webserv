<?php
	// createName.php

	$dataDir = __DIR__ . '/uploads/data';
	if (!is_dir($dataDir)) {
		mkdir($dataDir, 0777, true);
	}

	$name = preg_replace('/[^a-zA-Z0-9_-]/', '', $_POST['name']);
	$age = (int)$_POST['age'];

	if (!empty($name) && $age > 0) {
		file_put_contents($dataDir . '/' . $name . '.name', $age);
	}

	header('Location: /');
	exit();
?>
