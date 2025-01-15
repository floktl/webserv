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
		<h1>Hello, World!</h1>

		<h2>Cookie Test:</h2>
		<?php if (isset($_COOKIE['test_cookie'])): ?>
			<p>Cookie Value: <strong><?php echo htmlspecialchars($_COOKIE['test_cookie']); ?></strong></p>
		<?php else: ?>
			<p>No cookie set yet. Click button below to set one.</p>
		<?php endif; ?>

		<form action="createCookie.php" method="post">
			<input type="submit" value="Request Fresh Cookie">
		</form>

		<h2>Add Name and Age:</h2>
		<form action="createName.php" method="post">
			<input type="text" name="name" placeholder="name" required>
			<input type="number" name="age" placeholder="age" required>
			<input type="submit" value="Add">
		</form>
		<ul>
			<?php foreach ($names as $entry): ?>
				<li>
					<?php echo htmlspecialchars($entry['name']) . " (Age: " . htmlspecialchars($entry['age']) . ")"; ?>
					<button onclick="deleteName('<?php echo htmlspecialchars($entry['name']); ?>')" style="display: inline;">X</button>
				</li>
			<?php endforeach; ?>
		</ul>

		<hr>

		<h2>File Upload (Test):</h2>
		<form action="uploadFile.php" method="post" enctype="multipart/form-data">
			<input type="file" name="uploadedFile" required>
			<input type="submit" value="Upload File">
		</form>
		<ul>
			<?php foreach ($uploadedFiles as $fileName): ?>
				<li>
					<?php echo htmlspecialchars($fileName); ?>
					<button onclick="deleteFile('<?php echo htmlspecialchars($fileName); ?>')" style="display: inline;">X</button>
				</li>
			<?php endforeach; ?>
		</ul>

		<a href="/about.php">About Us</a>

		<script>
			function deleteName(name) {
				fetch("./data/" + name + ".name", {
					method: 'DELETE',
					headers: {
						'Content-Type': 'application/json',
					},
					body: JSON.stringify({ nameToDelete: name })
				})
				.then(response => {
					if (response.ok) {
						window.location.reload();
					}
				})
				.catch(error => console.error('Error:', error));
			}

			function deleteFile(name) {
				fetch("./" . $_SERVER["UPLOAD_STORE"] . "/" + name, {
					method: 'DELETE',
					headers: {
						'Content-Type': 'application/json',
					},
					body: JSON.stringify({ nameToDelete: name })
				})
				.then(response => {
					if (response.ok) {
						window.location.reload();
					}
				})
				.catch(error => console.error('Error:', error));
			}
		</script>
	</main>
</body>
</html>
