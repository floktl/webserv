<!DOCTYPE html>
<html lang="en">
<head>

	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<title>Welcome to My PHP Page</title>
	<style>
		body {
			font-family: Arial, sans-serif;
			text-align: center;
			background-color: #f4f4f9;
			margin: 0;
			padding: 0;
		}
		header {
			background-color: #4CAF50;
			color: white;
			padding: 20px 0;
			font-size: 2em;
		}
		main {
			padding: 20px;
		}
		a {
			color: #4CAF50;
			text-decoration: none;
			font-weight: bold;
		}
		a:hover {
			text-decoration: underline;
		}
		form {
			margin: 20px 0;
		}
		input[type="text"],
		input[type="number"] {
			padding: 10px;
			margin: 5px;
		}
		input[type="submit"] {
			background-color: #4CAF50;
			color: #fff;
			border: none;
			padding: 10px;
			cursor: pointer;
		}
		input[type="submit"]:hover {
			background-color: #45a049;
		}
		ul {
			padding: 0;
			list-style-type: none;
		}
		ul li {
			width: 50%;
			display: flex;
			gap: 20px;
			box-sizing: border-box;
			padding: 10px;
			margin: auto;
			justify-content: space-between;
		}
		ul li:nth-of-type(1n) {
			background-color: #ddd;
		}
		ul li:nth-of-type(2n) {
			background-color: white;
		}
	</style>
</head>
<body>
<?php
	// index.php

	$dataDir = __DIR__ . '/data';
	$names = [];
	// Create 'data' folder if it doesn't exist
	if (!is_dir($dataDir)) {
		mkdir($dataDir, 0777, true);
	}

	// Gather all name files
	if (is_dir($dataDir)) {
		$files = scandir($dataDir);
		foreach ($files as $file) {
			if (substr($file, -5) === '.name') {
				$name = substr($file, 0, -5);
				$age = trim(file_get_contents($dataDir . '/' . $file));
				$names[] = ['name' => $name, 'age' => $age];
			}
		}
	}

	// Create folder for uploaded files
	$filesDir = __DIR__ . '/upload';
	if (!is_dir($filesDir)) {
		mkdir($filesDir, 0777, true);
	}

	// List already-uploaded files
	$uploadedFiles = [];
	$allFiles = scandir($filesDir);
	foreach ($allFiles as $fl) {
		if ($fl !== '.' && $fl !== '..') {
			$uploadedFiles[] = $fl;
		}
	}
?>
	<header>
		Welcome to My PHP Page
	</header>
	<main>
		<h1>Hello, Beer!</h1>
		<?php for($i = 0; $i < 1000 ; $i++):?>
			<p><span style="color:red; font-weight:900;"><?php echo $i; ?></span> Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier Bier </p>
		<?php endfor;?>
	</main>
</body>
</html>
