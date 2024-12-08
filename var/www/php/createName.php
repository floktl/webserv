<?php

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	$dir = __DIR__ . '/data';
	if (!is_dir($dir)) {
		mkdir($dir, 0777, true);
	}

	$name = preg_replace('/[^a-zA-Z0-9_-]/', '', $_POST['name']);
	$age = (int)$_POST['age'];

	if (!empty($name) && $age > 0) {
		file_put_contents($dir . '/' . $name . '.name', $age);
	}
}

header('Location: /');
exit();